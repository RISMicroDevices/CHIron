#include "clog_b_trace_loader.hpp"

#include "chiron_b_adapter.hpp"
#include "chiron_eb_adapter.hpp"
#include "filter_parallel_utils.hpp"
#include "flit_transaction_keys.hpp"

#include "chi_b/util/chi_b_util_flit.hpp"
#include "chi_eb/xact/chi_eb_joint.hpp"
#include "chi_eb/xact/chi_eb_xact_state.hpp"
#include "chi_eb/util/chi_eb_util_flit.hpp"
#include "clog/clog_b/clog_b.hpp"

#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QStringList>

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <exception>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <unordered_map>
#include <system_error>
#include <thread>
#include <tuple>
#include <utility>
#include <variant>

namespace CHIron::Gui {

namespace {

using FlexibleEbConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 512, true, true, true>;
using FlexibleReqFlit = CHI::Eb::Flits::REQ<FlexibleEbConfig>;
using FlexibleRspFlit = CHI::Eb::Flits::RSP<FlexibleEbConfig>;
using FlexibleDatFlit = CHI::Eb::Flits::DAT<FlexibleEbConfig>;
using FlexibleSnpFlit = CHI::Eb::Flits::SNP<FlexibleEbConfig>;

template<std::size_t DataWidth>
using EbXactionConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, DataWidth, true, true, true>;

using FlexibleBConfig = CHI::B::FlitConfiguration<11, 52, 32, 32, 512, true, true>;
using FlexibleBReqFlit = CHI::B::Flits::REQ<FlexibleBConfig>;
using FlexibleBRspFlit = CHI::B::Flits::RSP<FlexibleBConfig>;
using FlexibleBDatFlit = CHI::B::Flits::DAT<FlexibleBConfig>;
using FlexibleBSnpFlit = CHI::B::Flits::SNP<FlexibleBConfig>;

struct EbMeasures {
    CHI::Eb::Flits::REQMeasure req;
    CHI::Eb::Flits::RSPMeasure rsp;
    CHI::Eb::Flits::DATMeasure dat;
    CHI::Eb::Flits::SNPMeasure snp;
};

struct BMeasures {
    CHI::B::Flits::REQMeasure req;
    CHI::B::Flits::RSPMeasure rsp;
    CHI::B::Flits::DATMeasure dat;
    CHI::B::Flits::SNPMeasure snp;
};

using DecodeProgressCallback =
    std::function<void(std::size_t completedRecords, std::size_t totalRecords)>;

inline const CompanionTypeBack<std::uint64_t> kXactionGuiOrdinalBack;
inline constexpr CompanionType<std::uint64_t> kXactionGuiOrdinal = &kXactionGuiOrdinalBack;

struct DecodedEbRecord {
    qint64 timestamp = 0;
    FlitDirection direction = FlitDirection::Tx;
    quint16 nodeId = 0;
    CLog::Channel sourceChannel = CLog::Channel::TXREQ;
    std::variant<std::monostate, FlexibleReqFlit, FlexibleRspFlit, FlexibleDatFlit, FlexibleSnpFlit> flit;
};

struct DecodedBRecord {
    qint64 timestamp = 0;
    FlitDirection direction = FlitDirection::Tx;
    quint16 nodeId = 0;
    std::variant<std::monostate, FlexibleBReqFlit, FlexibleBRspFlit, FlexibleBDatFlit, FlexibleBSnpFlit> flit;
};

QString issueName(const CLog::Issue issue)
{
    switch (issue) {
    case CLog::Issue::B:
        return QStringLiteral("B");
    case CLog::Issue::Eb:
        return QStringLiteral("E.b");
    }

    return QStringLiteral("Unknown");
}

QString boolName(const bool value)
{
    return value ? QStringLiteral("on")
                 : QStringLiteral("off");
}

QString channelName(CLog::Channel channel);
QString opcodeNameValue(const char* name);

void setLoadError(CLogBTraceLoadError& error,
                  const CLogBTraceLoadError::Type type,
                  QString summary,
                  QString informativeText = QString(),
                  QString detailedText = QString())
{
    error.type = type;
    error.summary = std::move(summary);
    error.informativeText = std::move(informativeText);
    error.detailedText = std::move(detailedText);
}

void setCancelledLoadError(CLogBTraceLoadError& error)
{
    setLoadError(error,
                 CLogBTraceLoadError::Type::Cancelled,
                 QStringLiteral("Loading was cancelled."),
                 QStringLiteral("The CLog.B load was interrupted before decoding completed."));
}

CHI::Eb::Xact::NodeTypeEnum toEbXactNodeType(const CLog::NodeType type) noexcept
{
    switch (type) {
    case CLog::NodeType::RN_F:
        return CHI::Eb::Xact::NodeType::RN_F;
    case CLog::NodeType::RN_D:
        return CHI::Eb::Xact::NodeType::RN_D;
    case CLog::NodeType::RN_I:
        return CHI::Eb::Xact::NodeType::RN_I;
    case CLog::NodeType::HN_F:
        return CHI::Eb::Xact::NodeType::HN_F;
    case CLog::NodeType::HN_I:
        return CHI::Eb::Xact::NodeType::HN_I;
    case CLog::NodeType::SN_F:
        return CHI::Eb::Xact::NodeType::SN_F;
    case CLog::NodeType::SN_I:
        return CHI::Eb::Xact::NodeType::SN_I;
    case CLog::NodeType::MN:
        return CHI::Eb::Xact::NodeType::MN;
    }
    return CHI::Eb::Xact::NodeType::RN_F;
}

void populateEbXactTopology(CHI::Eb::Xact::Topology& topology,
                            const std::unordered_map<quint16, CLog::NodeType>& nodeTypes)
{
    for (const auto& [nodeId, nodeType] : nodeTypes) {
        topology.SetNode(nodeId,
                         toEbXactNodeType(nodeType),
                         CLog::NodeTypeToString(nodeType));
    }
}

bool validNodeId(const std::uint64_t nodeId) noexcept
{
    return nodeId <= std::numeric_limits<std::uint16_t>::max();
}

bool topologyIsRequester(const CHI::Eb::Xact::Topology& topology,
                         const std::uint64_t nodeId) noexcept
{
    return validNodeId(nodeId) && topology.IsRequester(static_cast<std::uint16_t>(nodeId));
}

bool topologyIsSubordinate(const CHI::Eb::Xact::Topology& topology,
                           const std::uint64_t nodeId) noexcept
{
    return validNodeId(nodeId) && topology.IsSubordinate(static_cast<std::uint16_t>(nodeId));
}

void ensureEndpointNode(CHI::Eb::Xact::Topology& topology,
                        const std::uint64_t nodeId,
                        const CHI::Eb::Xact::NodeTypeEnum type)
{
    if (nodeId > std::numeric_limits<std::uint16_t>::max()) {
        return;
    }

    const auto narrowedNodeId = static_cast<std::uint16_t>(nodeId);
    if (!topology.HasNode(narrowedNodeId)) {
        topology.SetNode(narrowedNodeId, type, "inferred");
    }
}

QString ebSamScopeName(const CHI::Eb::Xact::SAMScopeEnum scope)
{
    return scope ? QString::fromLatin1(scope->name) : QStringLiteral("Unknown");
}

CHI::Eb::Xact::SAMScopeEnum ebSamScopeForReqChannel(const CLog::Channel channel) noexcept
{
    return (channel == CLog::Channel::TXREQ_BeforeSAM
            || channel == CLog::Channel::RXREQ_BeforeSAM)
        ? static_cast<CHI::Eb::Xact::SAMScopeEnum>(CHI::Eb::Xact::SAMScope::BeforeSAM)
        : static_cast<CHI::Eb::Xact::SAMScopeEnum>(CHI::Eb::Xact::SAMScope::AfterSAM);
}

template<typename Config>
void applyReqSamScope(CHI::Eb::Xact::Global<Config>& global,
                      const CLog::Channel channel,
                      const CHI::Eb::Flits::REQ<Config>& flit)
{
    if (!validNodeId(static_cast<std::uint64_t>(flit.SrcID()))) {
        return;
    }

    global.SAM_SCOPE.enable = true;
    global.SAM_SCOPE.Set(static_cast<std::uint16_t>(flit.SrcID()),
                         ebSamScopeForReqChannel(channel));
}

template<typename FlitT>
void ensureReqEndpointNodes(CHI::Eb::Xact::Topology& topology,
                            const FlitT& flit,
                            const CHI::Eb::Xact::SAMScopeEnum samScope)
{
    ensureEndpointNode(topology, static_cast<std::uint64_t>(flit.SrcID()), CHI::Eb::Xact::NodeType::RN_F);
    if (samScope == CHI::Eb::Xact::SAMScope::AfterSAM) {
        ensureEndpointNode(topology, static_cast<std::uint64_t>(flit.TgtID()), CHI::Eb::Xact::NodeType::HN_F);
    }
}

template<typename FlitT>
void ensureResponseEndpointNodes(CHI::Eb::Xact::Topology& topology, const FlitT& flit)
{
    ensureEndpointNode(topology, static_cast<std::uint64_t>(flit.TgtID()), CHI::Eb::Xact::NodeType::RN_F);
    ensureEndpointNode(topology, static_cast<std::uint64_t>(flit.SrcID()), CHI::Eb::Xact::NodeType::HN_F);
}

template<typename XactionT>
std::uint64_t assignGuiXactionOrdinal(const std::shared_ptr<XactionT>& xaction,
                                      std::uint64_t& nextOrdinal)
{
    if (!xaction) {
        return 0;
    }

    if (xaction->IsSecondTry()) {
        const std::shared_ptr<XactionT> firstTry = xaction->GetFirstTry();
        if (firstTry) {
            if (const std::optional<std::uint64_t> firstOrdinal =
                    firstTry->companion.Get(kXactionGuiOrdinal)) {
                xaction->companion.Set(kXactionGuiOrdinal, *firstOrdinal);
                return *firstOrdinal;
            }
        }
    }

    if (const std::optional<std::uint64_t> existing =
            xaction->companion.Get(kXactionGuiOrdinal)) {
        return *existing;
    }

    const std::uint64_t ordinal = nextOrdinal++;
    xaction->companion.Set(kXactionGuiOrdinal, ordinal);
    return ordinal;
}

void appendEbXactionGroupKey(FlitRecord& row, const std::uint64_t ordinal)
{
    if (ordinal == 0) {
        return;
    }

    AppendTransactionGroupKey(row,
                              QStringLiteral("xaction|%1")
                                  .arg(static_cast<qulonglong>(ordinal)));
}

template<typename Config>
using EbXactionPtr = std::shared_ptr<CHI::Eb::Xact::Xaction<Config>>;

template<typename Config>
struct EbXactionProcessResult {
    CHI::Eb::Xact::XactDenialEnum denial = nullptr;
    EbXactionPtr<Config> xaction;
    std::optional<std::uint64_t> ordinal;
    QString jointType;
    QString path;
    QStringList events;
    bool xactionComplete = false;
};

QString ebXactionDenialName(const CHI::Eb::Xact::XactDenialEnum denial)
{
    if (!denial) {
        return QStringLiteral("not processed");
    }
    return QString::fromLatin1(denial->name);
}

QString ebXactionDenialCode(const CHI::Eb::Xact::XactDenialEnum denial)
{
    if (!denial) {
        return QStringLiteral("-");
    }
    return QStringLiteral("0x%1").arg(QString::number(static_cast<unsigned int>(denial->value),
                                                      16).toUpper(),
                                      8,
                                      QLatin1Char('0'));
}

QString ebXactionDenialCategory(const CHI::Eb::Xact::XactDenialEnum denial)
{
    if (!denial) {
        return QStringLiteral("Not processed");
    }

    const QString name = ebXactionDenialName(denial);
    if (denial == CHI::Eb::Xact::XactDenial::ACCEPTED) {
        return QStringLiteral("Accepted");
    }
    if (denial == CHI::Eb::Xact::XactDenial::NOT_INITIALIZED) {
        return QStringLiteral("Internal state");
    }
    if (name.contains(QStringLiteral("_CHANNEL"))) {
        return QStringLiteral("Channel selection");
    }
    if (name.contains(QStringLiteral("_OPCODE"))) {
        return QStringLiteral("Opcode decoding/support");
    }
    if (name.contains(QStringLiteral("_TXNID"))) {
        return QStringLiteral("TxnID matching/allocation");
    }
    if (name.contains(QStringLiteral("_DBID"))) {
        return QStringLiteral("DBID matching/allocation");
    }
    if (name.contains(QStringLiteral("_FIELD_"))) {
        return QStringLiteral("Field legality");
    }
    if (name.contains(QStringLiteral("_STATE_"))) {
        return QStringLiteral("CHI state transition");
    }
    if (name.contains(QStringLiteral("_COMPLETED"))
        || name.contains(QStringLiteral("_AFTER_"))
        || name.contains(QStringLiteral("_BEFORE_"))
        || name.contains(QStringLiteral("_DUPLICATED"))) {
        return QStringLiteral("Ordering/state machine");
    }
    if (name.contains(QStringLiteral("_MATCHING"))
        || name.contains(QStringLiteral("_MISMATCH"))
        || name.contains(QStringLiteral("_NOT_FROM_"))
        || name.contains(QStringLiteral("_NOT_TO_"))) {
        return QStringLiteral("Endpoint matching");
    }
    if (name.contains(QStringLiteral("_UNSUPPORTED"))) {
        return QStringLiteral("Unsupported feature");
    }
    return QStringLiteral("Protocol denial");
}

QString ebXactionDenialDetail(const CHI::Eb::Xact::XactDenialEnum denial)
{
    if (!denial) {
        return QStringLiteral("The row did not call into a joint, usually because the channel/node direction could not be mapped.");
    }

    const QString name = ebXactionDenialName(denial);
    if (denial == CHI::Eb::Xact::XactDenial::ACCEPTED) {
        return QStringLiteral("The joint accepted this flit into an Xaction.");
    }
    if (denial == CHI::Eb::Xact::XactDenial::NOT_INITIALIZED) {
        return QStringLiteral("The Xaction object has not initialized its first-flit denial state.");
    }
    if (name.contains(QStringLiteral("_TXNID_IN_USE"))) {
        return QStringLiteral("A request/snoop with the same transaction ID is already open at this joint.");
    }
    if (name.contains(QStringLiteral("_TXNID_NOT_EXIST"))) {
        return QStringLiteral("No open Xaction matched this response/data flit's transaction ID.");
    }
    if (name.contains(QStringLiteral("_DBID_IN_USE"))) {
        return QStringLiteral("The DBID is already allocated to another still-open Xaction.");
    }
    if (name.contains(QStringLiteral("_DBID_NOT_EXIST"))) {
        return QStringLiteral("No open Xaction matched this data/response DBID.");
    }
    if (name.contains(QStringLiteral("_NO_MATCHING_RETRY"))) {
        return QStringLiteral("Retry sequencing was observed without a matching retried request.");
    }
    if (name.contains(QStringLiteral("_NO_MATCHING_PCREDIT"))) {
        return QStringLiteral("A request requiring a P-Credit did not find a matching grant.");
    }
    if (name.contains(QStringLiteral("_OPCODE_NOT_DECODED"))) {
        return QStringLiteral("The opcode could not be decoded by the CHI opcode decoder for this channel.");
    }
    if (name.contains(QStringLiteral("_OPCODE_NOT_SUPPORTED"))) {
        return QStringLiteral("The opcode decoded successfully, but this joint has no Xaction implementation for it.");
    }
    if (name.contains(QStringLiteral("_OPCODE"))) {
        return QStringLiteral("The opcode is not legal for the current Xaction state or channel.");
    }
    if (name.contains(QStringLiteral("_CHANNEL_NOT_"))) {
        return QStringLiteral("The flit was presented to an Xaction checker that expected a different CHI channel.");
    }
    if (name.contains(QStringLiteral("_CHANNEL_"))) {
        return QStringLiteral("The selected joint does not consume this transport channel in that direction.");
    }
    if (name.contains(QStringLiteral("_FIELD_"))) {
        return QStringLiteral("One or more CHI fields violate the opcode-specific field mapping rules.");
    }
    if (name.contains(QStringLiteral("_NOT_FROM_"))) {
        return QStringLiteral("The flit's source endpoint type does not match what this transaction state expects.");
    }
    if (name.contains(QStringLiteral("_NOT_TO_"))) {
        return QStringLiteral("The flit's target endpoint type does not match what this transaction state expects.");
    }
    if (name.contains(QStringLiteral("_MISMATCHING_"))
        || name.contains(QStringLiteral("_MISMATCH"))) {
        return QStringLiteral("An endpoint, TxnID, DBID, data ID, or state value does not match the tracked Xaction.");
    }
    if (name.contains(QStringLiteral("_COMPLETED"))) {
        return QStringLiteral("The matching Xaction was already complete before this flit was consumed.");
    }
    if (name.contains(QStringLiteral("_AFTER_"))) {
        return QStringLiteral("This flit arrived after a state that forbids it in the CHI transaction sequence.");
    }
    if (name.contains(QStringLiteral("_BEFORE_"))) {
        return QStringLiteral("This flit arrived before a required earlier transaction state.");
    }
    if (name.contains(QStringLiteral("_DUPLICATED"))) {
        return QStringLiteral("A repeated item such as DataID was already observed for this Xaction.");
    }
    if (name.contains(QStringLiteral("_STATE_"))) {
        return QStringLiteral("The CHI cache/state transition checker rejected this state transition.");
    }
    if (name.contains(QStringLiteral("_UNSUPPORTED"))) {
        return QStringLiteral("The Xaction library does not currently support this operation in this context.");
    }
    return QStringLiteral("The Xaction library returned this denial while checking the flit against active transaction state.");
}

QString ebJointDenialSourceName(const CHI::Eb::Xact::JointDenialSource source)
{
    switch (source) {
    case CHI::Eb::Xact::JointDenialSource::XACTION:
        return QStringLiteral("Xaction");
    case CHI::Eb::Xact::JointDenialSource::JOINT:
        return QStringLiteral("Joint");
    }
    return QStringLiteral("Unknown");
}

QString ebXactionTypeName(const CHI::Eb::Xact::XactionType type)
{
    switch (type) {
    case CHI::Eb::Xact::XactionType::AllocatingRead:
        return QStringLiteral("AllocatingRead");
    case CHI::Eb::Xact::XactionType::NonAllocatingRead:
        return QStringLiteral("NonAllocatingRead");
    case CHI::Eb::Xact::XactionType::ImmediateWrite:
        return QStringLiteral("ImmediateWrite");
    case CHI::Eb::Xact::XactionType::WriteZero:
        return QStringLiteral("WriteZero");
    case CHI::Eb::Xact::XactionType::CopyBackWrite:
        return QStringLiteral("CopyBackWrite");
    case CHI::Eb::Xact::XactionType::Atomic:
        return QStringLiteral("Atomic");
    case CHI::Eb::Xact::XactionType::IndependentStash:
        return QStringLiteral("IndependentStash");
    case CHI::Eb::Xact::XactionType::Dataless:
        return QStringLiteral("Dataless");
    case CHI::Eb::Xact::XactionType::HomeRead:
        return QStringLiteral("HomeRead");
    case CHI::Eb::Xact::XactionType::HomeWrite:
        return QStringLiteral("HomeWrite");
    case CHI::Eb::Xact::XactionType::HomeWriteZero:
        return QStringLiteral("HomeWriteZero");
    case CHI::Eb::Xact::XactionType::HomeDataless:
        return QStringLiteral("HomeDataless");
    case CHI::Eb::Xact::XactionType::HomeAtomic:
        return QStringLiteral("HomeAtomic");
    case CHI::Eb::Xact::XactionType::HomeSnoop:
        return QStringLiteral("HomeSnoop");
    case CHI::Eb::Xact::XactionType::ForwardSnoop:
        return QStringLiteral("ForwardSnoop");
    case CHI::Eb::Xact::XactionType::DVMSnoop:
        return QStringLiteral("DVMSnoop");
    }

    return QStringLiteral("Unknown");
}

QString ebXactionJointTypeFromPath(const QString& path)
{
    if (path.startsWith(QStringLiteral("RNFJoint::"))) {
        return QStringLiteral("RNFJoint");
    }
    if (path.startsWith(QStringLiteral("SNFJoint::"))) {
        return QStringLiteral("SNFJoint");
    }
    return QString();
}

template<typename Config>
QString ebFiredRequestFlitSummary(const CHI::Eb::Xact::FiredRequestFlit<Config>& flit)
{
    const QString direction = flit.IsTX() ? QStringLiteral("TX") : QStringLiteral("RX");
    if (flit.IsREQ()) {
        const QString opcode = opcodeNameValue(
            CHI::Eb::Opcodes::REQ::Decoder<CHI::Eb::Flits::REQ<Config>>::INSTANCE
                .Decode(flit.flit.req.Opcode())
                .GetName("Unknown"));
        return QStringLiteral("%1REQ %2 SrcID=%3 TgtID=%4 TxnID=%5")
            .arg(direction,
                 opcode,
                 QString::number(static_cast<qulonglong>(flit.flit.req.SrcID())),
                 QString::number(static_cast<qulonglong>(flit.flit.req.TgtID())),
                 QString::number(static_cast<qulonglong>(flit.flit.req.TxnID())));
    }

    const QString opcode = opcodeNameValue(
        CHI::Eb::Opcodes::SNP::Decoder<CHI::Eb::Flits::SNP<Config>>::INSTANCE
            .Decode(flit.flit.snp.Opcode())
            .GetName("Unknown"));
    return QStringLiteral("%1SNP %2 SrcID=%3 FwdNID=%4 TxnID=%5 FwdTxnID=%6 SnpTarget=%7")
        .arg(direction,
             opcode,
             QString::number(static_cast<qulonglong>(flit.flit.snp.SrcID())),
             QString::number(static_cast<qulonglong>(flit.flit.snp.FwdNID())),
             QString::number(static_cast<qulonglong>(flit.flit.snp.TxnID())),
             QString::number(static_cast<qulonglong>(flit.flit.snp.FwdTxnID())),
             QString::number(static_cast<qulonglong>(flit.nodeId)));
}

template<typename Config>
QString ebFiredResponseFlitSummary(const CHI::Eb::Xact::FiredResponseFlit<Config>& flit)
{
    const QString direction = flit.IsTX() ? QStringLiteral("TX") : QStringLiteral("RX");
    if (flit.IsRSP()) {
        const QString opcode = opcodeNameValue(
            CHI::Eb::Opcodes::RSP::Decoder<CHI::Eb::Flits::RSP<Config>>::INSTANCE
                .Decode(flit.flit.rsp.Opcode())
                .GetName("Unknown"));
        return QStringLiteral("%1RSP %2 SrcID=%3 TgtID=%4 TxnID=%5 DBID=%6")
            .arg(direction,
                 opcode,
                 QString::number(static_cast<qulonglong>(flit.flit.rsp.SrcID())),
                 QString::number(static_cast<qulonglong>(flit.flit.rsp.TgtID())),
                 QString::number(static_cast<qulonglong>(flit.flit.rsp.TxnID())),
                 QString::number(static_cast<qulonglong>(flit.flit.rsp.DBID())));
    }

    const QString opcode = opcodeNameValue(
        CHI::Eb::Opcodes::DAT::Decoder<CHI::Eb::Flits::DAT<Config>>::INSTANCE
            .Decode(flit.flit.dat.Opcode())
            .GetName("Unknown"));
    return QStringLiteral("%1DAT %2 SrcID=%3 TgtID=%4 TxnID=%5 DBID=%6 DataID=%7")
        .arg(direction,
             opcode,
             QString::number(static_cast<qulonglong>(flit.flit.dat.SrcID())),
             QString::number(static_cast<qulonglong>(flit.flit.dat.TgtID())),
             QString::number(static_cast<qulonglong>(flit.flit.dat.TxnID())),
             QString::number(static_cast<qulonglong>(flit.flit.dat.DBID())),
             QString::number(static_cast<qulonglong>(flit.flit.dat.DataID())));
}

template<typename Config>
std::uint64_t ebGuiOrdinalForXaction(const std::shared_ptr<CHI::Eb::Xact::Xaction<Config>>& xaction)
{
    if (!xaction) {
        return 0;
    }
    if (const std::optional<std::uint64_t> ordinal =
            xaction->companion.Get(kXactionGuiOrdinal)) {
        return *ordinal;
    }
    return 0;
}

template<typename Config>
QString ebXactionEventSummary(const QString& eventName,
                              const CHI::Eb::Xact::Joint<Config>& joint,
                              const std::shared_ptr<CHI::Eb::Xact::Xaction<Config>>& xaction)
{
    return QStringLiteral("%1 on %2 xaction=%3 type=%4")
        .arg(eventName,
             QString::fromLatin1(joint.GetType() ? joint.GetType()->name : "Unknown"),
             ebGuiOrdinalForXaction(xaction) == 0
                 ? QStringLiteral("pending")
                 : QStringLiteral("xaction|%1").arg(static_cast<qulonglong>(ebGuiOrdinalForXaction(xaction))),
             xaction ? ebXactionTypeName(xaction->GetType()) : QStringLiteral("none"));
}

template<typename Config>
QString ebJointDenialEventSummary(
    const QString& eventName,
    const CHI::Eb::Xact::Joint<Config>& joint,
    const std::shared_ptr<CHI::Eb::Xact::Xaction<Config>>& xaction,
    const CHI::Eb::Xact::XactDenialEnum denial,
    const CHI::Eb::Xact::JointDenialSource source,
    const std::string& message)
{
    QString summary = QStringLiteral("%1 on %2 source=%3 denial=%4")
        .arg(eventName,
             QString::fromLatin1(joint.GetType() ? joint.GetType()->name : "Unknown"),
             ebJointDenialSourceName(source),
             ebXactionDenialName(denial));
    if (xaction) {
        summary += QStringLiteral(" xaction=%1 type=%2")
            .arg(ebGuiOrdinalForXaction(xaction) == 0
                     ? QStringLiteral("pending")
                     : QStringLiteral("xaction|%1").arg(static_cast<qulonglong>(ebGuiOrdinalForXaction(xaction))),
                 ebXactionTypeName(xaction->GetType()));
    }
    if (!message.empty()) {
        summary += QStringLiteral(" message=\"%1\"").arg(QString::fromStdString(message));
    }
    return summary;
}

template<typename Config>
class EbXactionEventTrace final {
public:
    void install(CHI::Eb::Xact::RNFJoint<Config>& rnJoint,
                 CHI::Eb::Xact::SNFJoint<Config>& snJoint)
    {
        registerJoint(rnJoint, QStringLiteral("RNFJoint"));
        registerJoint(snJoint, QStringLiteral("SNFJoint"));
    }

    QStringList takeEvents()
    {
        QStringList taken = std::move(events_);
        events_.clear();
        return taken;
    }

private:
    void append(QString text)
    {
        events_.append(std::move(text));
        if (events_.size() > 128) {
            events_.removeFirst();
        }
    }

    void registerJoint(CHI::Eb::Xact::Joint<Config>& joint, const QString& label)
    {
        if (!joint.events) {
            return;
        }

        joint.events->OnDeniedRequest.Register(Gravity::MakeListener(
            std::string("clogan-denied-req-") + label.toStdString(),
            0,
            std::function([this, label](CHI::Eb::Xact::JointDeniedRequestEvent<Config>& event) {
                QString text = ebJointDenialEventSummary(label + QStringLiteral(" denied request"),
                                                         event.GetJoint(),
                                                         event.GetXaction(),
                                                         event.GetDenial(),
                                                         event.GetDenialSource(),
                                                         event.GetMessage());
                text += QStringLiteral(" flit={%1}").arg(ebFiredRequestFlitSummary(event.GetFiredFlit()));
                append(std::move(text));
            })));

        joint.events->OnDeniedResponse.Register(Gravity::MakeListener(
            std::string("clogan-denied-rsp-") + label.toStdString(),
            0,
            std::function([this, label](CHI::Eb::Xact::JointDeniedResponseEvent<Config>& event) {
                QString text = ebJointDenialEventSummary(label + QStringLiteral(" denied response"),
                                                         event.GetJoint(),
                                                         event.GetXaction(),
                                                         event.GetDenial(),
                                                         event.GetDenialSource(),
                                                         event.GetMessage());
                text += QStringLiteral(" flit={%1}").arg(ebFiredResponseFlitSummary(event.GetFiredFlit()));
                append(std::move(text));
            })));

        joint.events->OnAccepted.Register(Gravity::MakeListener(
            std::string("clogan-accepted-") + label.toStdString(),
            0,
            std::function([this, label](CHI::Eb::Xact::JointXactionAcceptedEvent<Config>& event) {
                append(ebXactionEventSummary(label + QStringLiteral(" accepted"),
                                             event.GetJoint(),
                                             event.GetXaction()));
            })));

        joint.events->OnRetry.Register(Gravity::MakeListener(
            std::string("clogan-retry-") + label.toStdString(),
            0,
            std::function([this, label](CHI::Eb::Xact::JointXactionRetryEvent<Config>& event) {
                append(ebXactionEventSummary(label + QStringLiteral(" retry"),
                                             event.GetJoint(),
                                             event.GetXaction()));
            })));

        joint.events->OnTxnIDAllocation.Register(Gravity::MakeListener(
            std::string("clogan-txn-alloc-") + label.toStdString(),
            0,
            std::function([this, label](CHI::Eb::Xact::JointXactionTxnIDAllocationEvent<Config>& event) {
                append(ebXactionEventSummary(label + QStringLiteral(" TxnID allocated"),
                                             event.GetJoint(),
                                             event.GetXaction()));
            })));

        joint.events->OnTxnIDFree.Register(Gravity::MakeListener(
            std::string("clogan-txn-free-") + label.toStdString(),
            0,
            std::function([this, label](CHI::Eb::Xact::JointXactionTxnIDFreeEvent<Config>& event) {
                append(ebXactionEventSummary(label + QStringLiteral(" TxnID freed"),
                                             event.GetJoint(),
                                             event.GetXaction()));
            })));

        joint.events->OnDBIDAllocation.Register(Gravity::MakeListener(
            std::string("clogan-dbid-alloc-") + label.toStdString(),
            0,
            std::function([this, label](CHI::Eb::Xact::JointXactionDBIDAllocationEvent<Config>& event) {
                append(ebXactionEventSummary(label + QStringLiteral(" DBID allocated"),
                                             event.GetJoint(),
                                             event.GetXaction()));
            })));

        joint.events->OnDBIDFree.Register(Gravity::MakeListener(
            std::string("clogan-dbid-free-") + label.toStdString(),
            0,
            std::function([this, label](CHI::Eb::Xact::JointXactionDBIDFreeEvent<Config>& event) {
                append(ebXactionEventSummary(label + QStringLiteral(" DBID freed"),
                                             event.GetJoint(),
                                             event.GetXaction()));
            })));

        joint.events->OnComplete.Register(Gravity::MakeListener(
            std::string("clogan-complete-") + label.toStdString(),
            0,
            std::function([this, label](CHI::Eb::Xact::JointXactionCompleteEvent<Config>& event) {
                append(ebXactionEventSummary(label + QStringLiteral(" complete"),
                                             event.GetJoint(),
                                             event.GetXaction()));
            })));
    }

private:
    QStringList events_;
};

template<typename Config>
EbXactionProcessResult<Config> finishEbXactionProcess(
    const CHI::Eb::Xact::Global<Config>& global,
    const CHI::Eb::Xact::XactDenialEnum denial,
    const EbXactionPtr<Config>& xaction,
    std::uint64_t& nextOrdinal,
    QString path,
    QStringList events = {})
{
    EbXactionProcessResult<Config> result;
    result.denial = denial;
    result.xaction = xaction;
    result.path = std::move(path);
    result.jointType = ebXactionJointTypeFromPath(result.path);
    if (denial == CHI::Eb::Xact::XactDenial::ACCEPTED && xaction) {
        result.ordinal = assignGuiXactionOrdinal(xaction, nextOrdinal);
        result.xactionComplete = xaction->IsComplete(global);
    }
    result.events = std::move(events);

    return result;
}

template<typename Config>
QString buildEbXactionDebugLog(
    const CHI::Eb::Xact::Global<Config>& global,
    const DecodedEbRecord& decodedRecord,
    const FlitRecord& row,
    const EbXactionProcessResult<Config>& result)
{
    QStringList lines;
    lines << QStringLiteral("Xaction highlight tracking");
    lines << QStringLiteral("  Trace data width: %1").arg(Config::dataWidth);
    lines << QStringLiteral("  Observed row: time=%1 node=%2 %3%4 %5")
                 .arg(row.timestamp)
                 .arg(decodedRecord.nodeId)
                 .arg(ToString(row.direction), ToString(row.channel), row.opcode);
    lines << QStringLiteral("  CLog channel: %1").arg(channelName(decodedRecord.sourceChannel));
    lines << QStringLiteral("  SrcID=%1 TgtID=%2 TxnID=%3 DBID=%4 DataID=%5")
                 .arg(row.source.isEmpty() ? QStringLiteral("-") : row.source,
                      row.target.isEmpty() ? QStringLiteral("-") : row.target,
                      row.txnId.isEmpty() ? QStringLiteral("-") : row.txnId,
                      row.dbid.isEmpty() ? QStringLiteral("-") : row.dbid,
                      row.dataId.isEmpty() ? QStringLiteral("-") : row.dataId);
    lines << QStringLiteral("  Joint path: %1")
                 .arg(result.path.isEmpty() ? QStringLiteral("not processed") : result.path);
    lines << QStringLiteral("  Denial: %1 (%2)").arg(ebXactionDenialName(result.denial),
                                                      ebXactionDenialCode(result.denial));
    lines << QStringLiteral("  Denial category: %1").arg(ebXactionDenialCategory(result.denial));
    lines << QStringLiteral("  Denial detail: %1").arg(ebXactionDenialDetail(result.denial));

    if (result.ordinal) {
        lines << QStringLiteral("  Xaction key: xaction|%1")
                     .arg(static_cast<qulonglong>(*result.ordinal));
    } else {
        lines << QStringLiteral("  Xaction key: none");
    }

    if (result.xaction) {
        lines << QStringLiteral("  Xaction type: %1")
                     .arg(ebXactionTypeName(result.xaction->GetType()));
        lines << QStringLiteral("  Complete after flit: %1")
                     .arg(boolName(result.xaction->IsComplete(global)));
        lines << QStringLiteral("  TxnID complete after flit: %1")
                     .arg(boolName(result.xaction->IsTxnIDComplete(global)));
        lines << QStringLiteral("  DBID complete after flit: %1")
                     .arg(boolName(result.xaction->IsDBIDComplete(global)));
        lines << QStringLiteral("  Second try: %1").arg(boolName(result.xaction->IsSecondTry()));
        lines << QStringLiteral("  Accepted response flits tracked: %1")
                     .arg(static_cast<qulonglong>(result.xaction->GetSubsequence().size()));
        lines << QStringLiteral("  First flit: %1")
                     .arg(ebFiredRequestFlitSummary(result.xaction->GetFirst()));
        lines << QStringLiteral("  First denial: %1 (%2)")
                     .arg(ebXactionDenialName(result.xaction->GetFirstDenial()),
                          ebXactionDenialCode(result.xaction->GetFirstDenial()));
        lines << QStringLiteral("  Last denial: %1")
                     .arg(ebXactionDenialName(result.xaction->GetLastDenial()));
        if (!result.xaction->GetSubsequence().empty()) {
            lines << QStringLiteral("  Tracked response subsequence:");
            for (std::size_t index = 0; index < result.xaction->GetSubsequence().size(); ++index) {
                lines << QStringLiteral("    [%1] %2 denial=%3")
                             .arg(static_cast<qulonglong>(index))
                             .arg(ebFiredResponseFlitSummary(result.xaction->GetSubsequence()[index]))
                             .arg(ebXactionDenialName(result.xaction->GetSubsequentDenial(index)));
            }
        }
    } else {
        lines << QStringLiteral("  Xaction type: none");
    }

    lines << QStringLiteral("  Joint events emitted by this flit:");
    if (result.events.isEmpty()) {
        lines << QStringLiteral("    (none)");
    } else {
        for (const QString& event : result.events) {
            lines << QStringLiteral("    %1").arg(event);
        }
    }

    return lines.join(QLatin1Char('\n'));
}

template<typename Config>
QString buildEbXactionSkippedDebugLog(const DecodedEbRecord& decodedRecord,
                                      const FlitRecord& row)
{
    QStringList lines;
    lines << QStringLiteral("Xaction highlight tracking");
    lines << QStringLiteral("  Trace data width: %1").arg(Config::dataWidth);
    lines << QStringLiteral("  Observed row: time=%1 node=%2 %3%4 %5")
                 .arg(row.timestamp)
                 .arg(decodedRecord.nodeId)
                 .arg(ToString(row.direction), ToString(row.channel), row.opcode);
    lines << QStringLiteral("  SrcID=%1 TgtID=%2 TxnID=%3 DBID=%4 DataID=%5")
                 .arg(row.source.isEmpty() ? QStringLiteral("-") : row.source,
                      row.target.isEmpty() ? QStringLiteral("-") : row.target,
                      row.txnId.isEmpty() ? QStringLiteral("-") : row.txnId,
                      row.dbid.isEmpty() ? QStringLiteral("-") : row.dbid,
                      row.dataId.isEmpty() ? QStringLiteral("-") : row.dataId);
    lines << QStringLiteral("  Joint path: skipped");
    lines << QStringLiteral("  Denial: not processed");
    lines << QStringLiteral("  Xaction key: none");
    lines << QStringLiteral("  Reason: random-access row decoding skips RNFJoint/SNFJoint tracking because the required earlier flit history may be outside this decoded page.");
    return lines.join(QLatin1Char('\n'));
}

void appendTransactionKeysToXactionDebugLog(QString& log, const FlitRecord& row)
{
    if (log.isEmpty()) {
        return;
    }

    const QStringList keys = TransactionGroupKeys(row);
    log += QStringLiteral("\nTransaction keys after formatting:");
    if (keys.isEmpty()) {
        log += QStringLiteral("\n  (none)");
        return;
    }
    for (const QString& key : keys) {
        log += QStringLiteral("\n  ");
        log += key;
    }
}

template<typename Config>
CHI::Eb::Flits::REQ<Config> makeEbXactionReqFlit(const FlexibleReqFlit& source)
{
    CHI::Eb::Flits::REQ<Config> target;
    target.QoS() = source.QoS();
    target.TgtID() = source.TgtID();
    target.SrcID() = source.SrcID();
    target.TxnID() = source.TxnID();
    target.ReturnNID() = source.ReturnNID();
    target.StashNIDValid() = source.StashNIDValid();
    target.ReturnTxnID() = source.ReturnTxnID();
    target.Opcode() = source.Opcode();
    target.Size() = source.Size();
    target.Addr() = source.Addr();
    target.NS() = source.NS();
    target.LikelyShared() = source.LikelyShared();
    target.AllowRetry() = source.AllowRetry();
    target.Order() = source.Order();
    target.PCrdType() = source.PCrdType();
    target.MemAttr() = source.MemAttr();
    target.SnpAttr() = source.SnpAttr();
    target.TagGroupID() = source.TagGroupID();
    target.Excl() = source.Excl();
    target.ExpCompAck() = source.ExpCompAck();
    target.TagOp() = source.TagOp();
    target.TraceTag() = source.TraceTag();
    if constexpr (CHI::Eb::Flits::REQ<Config>::hasMPAM) {
        target.MPAM() = source.MPAM();
    }
    if constexpr (CHI::Eb::Flits::REQ<Config>::hasRSVDC) {
        target.RSVDC() = source.RSVDC();
    }
    return target;
}

template<typename Config>
CHI::Eb::Flits::RSP<Config> makeEbXactionRspFlit(const FlexibleRspFlit& source)
{
    CHI::Eb::Flits::RSP<Config> target;
    target.QoS() = source.QoS();
    target.TgtID() = source.TgtID();
    target.SrcID() = source.SrcID();
    target.TxnID() = source.TxnID();
    target.Opcode() = source.Opcode();
    target.RespErr() = source.RespErr();
    target.Resp() = source.Resp();
    target.FwdState() = source.FwdState();
    target.CBusy() = source.CBusy();
    target.DBID() = source.DBID();
    target.PCrdType() = source.PCrdType();
    target.TagOp() = source.TagOp();
    target.TraceTag() = source.TraceTag();
    return target;
}

template<typename Config>
CHI::Eb::Flits::DAT<Config> makeEbXactionDatFlit(const FlexibleDatFlit& source)
{
    using Target = CHI::Eb::Flits::DAT<Config>;

    Target target;
    target.QoS() = source.QoS();
    target.TgtID() = source.TgtID();
    target.SrcID() = source.SrcID();
    target.TxnID() = source.TxnID();
    target.HomeNID() = source.HomeNID();
    target.Opcode() = source.Opcode();
    target.RespErr() = source.RespErr();
    target.Resp() = source.Resp();
    target.DataSource() = source.DataSource();
    target.CBusy() = source.CBusy();
    target.DBID() = source.DBID();
    target.CCID() = source.CCID();
    target.DataID() = source.DataID();
    target.TagOp() = source.TagOp();
    target.Tag() = source.Tag();
    target.TU() = source.TU();
    target.TraceTag() = source.TraceTag();
    if constexpr (Target::hasRSVDC) {
        target.RSVDC() = source.RSVDC();
    }
    target.BE() = source.BE();

    const std::size_t wordCount = std::min<std::size_t>(Target::DATA_WIDTH / 64,
                                                        FlexibleDatFlit::DATA_WIDTH / 64);
    for (std::size_t index = 0; index < wordCount; ++index) {
        target.Data()[index] = source.Data()[index];
    }
    if constexpr (Target::hasDataCheck) {
        target.DataCheck() = source.DataCheck();
    }
    if constexpr (Target::hasPoison) {
        target.Poison() = source.Poison();
    }
    return target;
}

template<typename Config>
CHI::Eb::Flits::SNP<Config> makeEbXactionSnpFlit(const FlexibleSnpFlit& source)
{
    CHI::Eb::Flits::SNP<Config> target;
    target.QoS() = source.QoS();
    target.SrcID() = source.SrcID();
    target.TxnID() = source.TxnID();
    target.FwdNID() = source.FwdNID();
    target.FwdTxnID() = source.FwdTxnID();
    target.Opcode() = source.Opcode();
    target.Addr() = source.Addr();
    target.NS() = source.NS();
    target.DoNotGoToSD() = source.DoNotGoToSD();
    target.RetToSrc() = source.RetToSrc();
    target.TraceTag() = source.TraceTag();
    if constexpr (CHI::Eb::Flits::SNP<Config>::hasMPAM) {
        target.MPAM() = source.MPAM();
    }
    return target;
}

template<typename Config>
EbXactionProcessResult<Config> processEbReqXaction(
    CHI::Eb::Xact::Global<Config>& global,
    CHI::Eb::Xact::Topology& topology,
    CHI::Eb::Xact::RNFJoint<Config>& rnJoint,
    CHI::Eb::Xact::SNFJoint<Config>& snJoint,
    const DecodedEbRecord& decodedRecord,
    const CHI::Eb::Flits::REQ<Config>& flit,
    std::uint64_t& nextOrdinal)
{
    const CHI::Eb::Xact::SAMScopeEnum samScope =
        ebSamScopeForReqChannel(decodedRecord.sourceChannel);
    applyReqSamScope(global, decodedRecord.sourceChannel, flit);
    ensureReqEndpointNodes(topology, flit, samScope);

    EbXactionPtr<Config> xaction;
    CHI::Eb::Xact::XactDenialEnum denial = nullptr;
    QString path;
    const QString samSuffix = QStringLiteral(" [%1]").arg(ebSamScopeName(samScope));
    if (topology.IsRequester(decodedRecord.nodeId)) {
        if (decodedRecord.direction == FlitDirection::Tx) {
            path = QStringLiteral("RNFJoint::NextTXREQ") + samSuffix;
            denial = rnJoint.NextTXREQ(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
        }
    } else if (topology.IsSubordinate(decodedRecord.nodeId)) {
        if (decodedRecord.direction == FlitDirection::Rx) {
            path = QStringLiteral("SNFJoint::NextRXREQ") + samSuffix;
            denial = snJoint.NextRXREQ(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
        }
    } else if (topology.IsHome(decodedRecord.nodeId)) {
        if (decodedRecord.direction == FlitDirection::Rx) {
            path = QStringLiteral("RNFJoint::NextTXREQ") + samSuffix;
            denial = rnJoint.NextTXREQ(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
        } else {
            path = QStringLiteral("SNFJoint::NextRXREQ") + samSuffix;
            denial = snJoint.NextRXREQ(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
        }
    }

    return finishEbXactionProcess(global, denial, xaction, nextOrdinal, std::move(path));
}

template<typename Config>
EbXactionProcessResult<Config> processEbRspXaction(
    CHI::Eb::Xact::Global<Config>& global,
    CHI::Eb::Xact::Topology& topology,
    CHI::Eb::Xact::RNFJoint<Config>& rnJoint,
    CHI::Eb::Xact::SNFJoint<Config>& snJoint,
    const DecodedEbRecord& decodedRecord,
    const CHI::Eb::Flits::RSP<Config>& flit,
    std::uint64_t& nextOrdinal)
{
    ensureResponseEndpointNodes(topology, flit);

    EbXactionPtr<Config> xaction;
    CHI::Eb::Xact::XactDenialEnum denial = nullptr;
    QString path;
    if (topology.IsRequester(decodedRecord.nodeId)) {
        if (decodedRecord.direction == FlitDirection::Tx) {
            path = QStringLiteral("RNFJoint::NextTXRSP");
            denial = rnJoint.NextTXRSP(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
        } else {
            path = QStringLiteral("RNFJoint::NextRXRSP");
            denial = rnJoint.NextRXRSP(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
        }
    } else if (topology.IsSubordinate(decodedRecord.nodeId)) {
        if (decodedRecord.direction == FlitDirection::Tx) {
            path = QStringLiteral("SNFJoint::NextTXRSP");
            denial = snJoint.NextTXRSP(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
        }
    } else if (topology.IsHome(decodedRecord.nodeId)) {
        if (decodedRecord.direction == FlitDirection::Rx) {
            if (topologyIsRequester(topology, static_cast<std::uint64_t>(flit.SrcID()))) {
                path = QStringLiteral("RNFJoint::NextTXRSP");
                denial = rnJoint.NextTXRSP(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
            } else if (topologyIsSubordinate(topology, static_cast<std::uint64_t>(flit.SrcID()))) {
                path = QStringLiteral("SNFJoint::NextTXRSP");
                denial = snJoint.NextTXRSP(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
            }
        } else {
            if (topologyIsRequester(topology, static_cast<std::uint64_t>(flit.TgtID()))) {
                path = QStringLiteral("RNFJoint::NextRXRSP");
                denial = rnJoint.NextRXRSP(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
            } else if (topologyIsSubordinate(topology, static_cast<std::uint64_t>(flit.TgtID()))) {
                path = QStringLiteral("SNFJoint::NextRXRSP");
                denial = snJoint.NextRXRSP(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
            }
        }
    }

    return finishEbXactionProcess(global, denial, xaction, nextOrdinal, std::move(path));
}

template<typename Config>
EbXactionProcessResult<Config> processEbDatXaction(
    CHI::Eb::Xact::Global<Config>& global,
    CHI::Eb::Xact::Topology& topology,
    CHI::Eb::Xact::RNFJoint<Config>& rnJoint,
    CHI::Eb::Xact::SNFJoint<Config>& snJoint,
    const DecodedEbRecord& decodedRecord,
    const CHI::Eb::Flits::DAT<Config>& flit,
    std::uint64_t& nextOrdinal)
{
    ensureResponseEndpointNodes(topology, flit);

    EbXactionPtr<Config> xaction;
    CHI::Eb::Xact::XactDenialEnum denial = nullptr;
    QString path;
    if (topology.IsRequester(decodedRecord.nodeId)) {
        if (decodedRecord.direction == FlitDirection::Tx) {
            path = QStringLiteral("RNFJoint::NextTXDAT");
            denial = rnJoint.NextTXDAT(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
        } else {
            path = QStringLiteral("RNFJoint::NextRXDAT");
            denial = rnJoint.NextRXDAT(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
        }
    } else if (topology.IsSubordinate(decodedRecord.nodeId)) {
        if (decodedRecord.direction == FlitDirection::Tx) {
            path = QStringLiteral("SNFJoint::NextTXDAT");
            denial = snJoint.NextTXDAT(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
        } else {
            path = QStringLiteral("SNFJoint::NextRXDAT");
            denial = snJoint.NextRXDAT(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
        }
    } else if (topology.IsHome(decodedRecord.nodeId)) {
        if (decodedRecord.direction == FlitDirection::Rx) {
            if (topologyIsRequester(topology, static_cast<std::uint64_t>(flit.SrcID()))) {
                path = QStringLiteral("RNFJoint::NextTXDAT");
                denial = rnJoint.NextTXDAT(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
            } else if (topologyIsSubordinate(topology, static_cast<std::uint64_t>(flit.SrcID()))) {
                path = QStringLiteral("SNFJoint::NextTXDAT");
                denial = snJoint.NextTXDAT(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
            }
        } else {
            if (topologyIsRequester(topology, static_cast<std::uint64_t>(flit.TgtID()))) {
                path = QStringLiteral("RNFJoint::NextRXDAT");
                denial = rnJoint.NextRXDAT(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
            } else if (topologyIsSubordinate(topology, static_cast<std::uint64_t>(flit.TgtID()))) {
                path = QStringLiteral("SNFJoint::NextRXDAT");
                denial = snJoint.NextRXDAT(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
            }
        }
    }

    return finishEbXactionProcess(global, denial, xaction, nextOrdinal, std::move(path));
}

template<typename Config>
EbXactionProcessResult<Config> processEbSnpXaction(
    CHI::Eb::Xact::Global<Config>& global,
    CHI::Eb::Xact::Topology& topology,
    CHI::Eb::Xact::RNFJoint<Config>& rnJoint,
    const DecodedEbRecord& decodedRecord,
    const CHI::Eb::Flits::SNP<Config>& flit,
    std::uint64_t& nextOrdinal)
{
    ensureEndpointNode(topology, decodedRecord.nodeId, CHI::Eb::Xact::NodeType::RN_F);
    ensureEndpointNode(topology, static_cast<std::uint64_t>(flit.SrcID()), CHI::Eb::Xact::NodeType::HN_F);

    EbXactionPtr<Config> xaction;
    CHI::Eb::Xact::XactDenialEnum denial = nullptr;
    QString path;
    if (topology.IsRequester(decodedRecord.nodeId)
        && decodedRecord.direction == FlitDirection::Rx) {
        path = QStringLiteral("RNFJoint::NextRXSNP");
        denial = rnJoint.NextRXSNP(global,
                                   static_cast<std::uint64_t>(decodedRecord.timestamp),
                                   flit,
                                   decodedRecord.nodeId,
                                   &xaction);
    }

    return finishEbXactionProcess(global, denial, xaction, nextOrdinal, std::move(path));
}

bool ebXactionUseRnfJoint(const CLog::NodeType type) noexcept
{
    switch (type) {
    case CLog::NodeType::RN_F:
    case CLog::NodeType::RN_D:
    case CLog::NodeType::RN_I:
        return true;
    case CLog::NodeType::HN_F:
    case CLog::NodeType::HN_I:
    case CLog::NodeType::SN_F:
    case CLog::NodeType::SN_I:
    case CLog::NodeType::MN:
        return false;
    }

    return false;
}

bool ebXactionUseSnfJoint(const CLog::NodeType type) noexcept
{
    switch (type) {
    case CLog::NodeType::HN_F:
    case CLog::NodeType::HN_I:
    case CLog::NodeType::SN_F:
    case CLog::NodeType::SN_I:
    case CLog::NodeType::MN:
        return true;
    case CLog::NodeType::RN_F:
    case CLog::NodeType::RN_D:
    case CLog::NodeType::RN_I:
        return false;
    }

    return false;
}

std::optional<CLog::NodeType> ebXactionNodeTypeForRecord(
    const std::unordered_map<quint16, CLog::NodeType>& nodeTypes,
    const DecodedEbRecord& decodedRecord)
{
    const auto found = nodeTypes.find(decodedRecord.nodeId);
    if (found == nodeTypes.end()) {
        return std::nullopt;
    }

    return found->second;
}

QString ebXactionJointPath(const char* joint,
                           const char* method,
                           const CLog::NodeType nodeType)
{
    return QStringLiteral("%1::%2 [record node type: %3]")
        .arg(QString::fromLatin1(joint),
             QString::fromLatin1(method),
             QString::fromStdString(CLog::NodeTypeToString(nodeType)));
}

QString ebXactionReqJointPath(const char* joint,
                              const char* method,
                              const CLog::NodeType nodeType,
                              const CHI::Eb::Xact::SAMScopeEnum samScope)
{
    return QStringLiteral("%1::%2 [record node type: %3, SAM: %4]")
        .arg(QString::fromLatin1(joint),
             QString::fromLatin1(method),
             QString::fromStdString(CLog::NodeTypeToString(nodeType)),
             ebSamScopeName(samScope));
}

template<typename Config>
EbXactionProcessResult<Config> finishEbXactionNotIndexed(QString path = QString())
{
    EbXactionProcessResult<Config> result;
    result.path = std::move(path);
    result.jointType = ebXactionJointTypeFromPath(result.path);
    return result;
}

template<typename Config>
EbXactionProcessResult<Config> processEbReqXactionByRecordNode(
    CHI::Eb::Xact::Global<Config>& global,
    CHI::Eb::Xact::Topology& topology,
    CHI::Eb::Xact::RNFJoint<Config>& rnJoint,
    CHI::Eb::Xact::SNFJoint<Config>& snJoint,
    const std::unordered_map<quint16, CLog::NodeType>& nodeTypes,
    const DecodedEbRecord& decodedRecord,
    const CHI::Eb::Flits::REQ<Config>& flit,
    std::uint64_t& nextOrdinal)
{
    const std::optional<CLog::NodeType> nodeType =
        ebXactionNodeTypeForRecord(nodeTypes, decodedRecord);
    if (!nodeType) {
        return finishEbXactionNotIndexed<Config>(
            QStringLiteral("not processed (record nodeId is not listed in topology)"));
    }

    const CHI::Eb::Xact::SAMScopeEnum samScope =
        ebSamScopeForReqChannel(decodedRecord.sourceChannel);
    applyReqSamScope(global, decodedRecord.sourceChannel, flit);
    ensureReqEndpointNodes(topology, flit, samScope);

    EbXactionPtr<Config> xaction;
    CHI::Eb::Xact::XactDenialEnum denial = nullptr;
    QString path;

    if (ebXactionUseRnfJoint(*nodeType)) {
        if (decodedRecord.direction == FlitDirection::Tx) {
            path = ebXactionReqJointPath("RNFJoint", "NextTXREQ", *nodeType, samScope);
            denial = rnJoint.NextTXREQ(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
        } else {
            path = ebXactionReqJointPath("RNFJoint", "NextRXREQ", *nodeType, samScope);
            denial = rnJoint.NextRXREQ(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
        }
    } else if (ebXactionUseSnfJoint(*nodeType)) {
        if (decodedRecord.direction == FlitDirection::Tx) {
            path = ebXactionReqJointPath("SNFJoint", "NextTXREQ", *nodeType, samScope);
            denial = snJoint.NextTXREQ(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
        } else {
            path = ebXactionReqJointPath("SNFJoint", "NextRXREQ", *nodeType, samScope);
            denial = snJoint.NextRXREQ(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
        }
    }

    return finishEbXactionProcess(global, denial, xaction, nextOrdinal, std::move(path));
}

template<typename Config>
EbXactionProcessResult<Config> processEbRspXactionByRecordNode(
    CHI::Eb::Xact::Global<Config>& global,
    CHI::Eb::Xact::Topology& topology,
    CHI::Eb::Xact::RNFJoint<Config>& rnJoint,
    CHI::Eb::Xact::SNFJoint<Config>& snJoint,
    const std::unordered_map<quint16, CLog::NodeType>& nodeTypes,
    const DecodedEbRecord& decodedRecord,
    const CHI::Eb::Flits::RSP<Config>& flit,
    std::uint64_t& nextOrdinal)
{
    const std::optional<CLog::NodeType> nodeType =
        ebXactionNodeTypeForRecord(nodeTypes, decodedRecord);
    if (!nodeType) {
        return finishEbXactionNotIndexed<Config>(
            QStringLiteral("not processed (record nodeId is not listed in topology)"));
    }

    ensureResponseEndpointNodes(topology, flit);

    EbXactionPtr<Config> xaction;
    CHI::Eb::Xact::XactDenialEnum denial = nullptr;
    QString path;

    if (ebXactionUseRnfJoint(*nodeType)) {
        if (decodedRecord.direction == FlitDirection::Tx) {
            path = ebXactionJointPath("RNFJoint", "NextTXRSP", *nodeType);
            denial = rnJoint.NextTXRSP(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
        } else {
            path = ebXactionJointPath("RNFJoint", "NextRXRSP", *nodeType);
            denial = rnJoint.NextRXRSP(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
        }
    } else if (ebXactionUseSnfJoint(*nodeType)) {
        if (decodedRecord.direction == FlitDirection::Tx) {
            path = ebXactionJointPath("SNFJoint", "NextTXRSP", *nodeType);
            denial = snJoint.NextTXRSP(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
        } else {
            path = ebXactionJointPath("SNFJoint", "NextRXRSP", *nodeType);
            denial = snJoint.NextRXRSP(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
        }
    }

    return finishEbXactionProcess(global, denial, xaction, nextOrdinal, std::move(path));
}

template<typename Config>
EbXactionProcessResult<Config> processEbDatXactionByRecordNode(
    CHI::Eb::Xact::Global<Config>& global,
    CHI::Eb::Xact::Topology& topology,
    CHI::Eb::Xact::RNFJoint<Config>& rnJoint,
    CHI::Eb::Xact::SNFJoint<Config>& snJoint,
    const std::unordered_map<quint16, CLog::NodeType>& nodeTypes,
    const DecodedEbRecord& decodedRecord,
    const CHI::Eb::Flits::DAT<Config>& flit,
    std::uint64_t& nextOrdinal)
{
    const std::optional<CLog::NodeType> nodeType =
        ebXactionNodeTypeForRecord(nodeTypes, decodedRecord);
    if (!nodeType) {
        return finishEbXactionNotIndexed<Config>(
            QStringLiteral("not processed (record nodeId is not listed in topology)"));
    }

    ensureResponseEndpointNodes(topology, flit);

    EbXactionPtr<Config> xaction;
    CHI::Eb::Xact::XactDenialEnum denial = nullptr;
    QString path;

    if (ebXactionUseRnfJoint(*nodeType)) {
        if (decodedRecord.direction == FlitDirection::Tx) {
            path = ebXactionJointPath("RNFJoint", "NextTXDAT", *nodeType);
            denial = rnJoint.NextTXDAT(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
        } else {
            path = ebXactionJointPath("RNFJoint", "NextRXDAT", *nodeType);
            denial = rnJoint.NextRXDAT(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
        }
    } else if (ebXactionUseSnfJoint(*nodeType)) {
        if (decodedRecord.direction == FlitDirection::Tx) {
            path = ebXactionJointPath("SNFJoint", "NextTXDAT", *nodeType);
            denial = snJoint.NextTXDAT(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
        } else {
            path = ebXactionJointPath("SNFJoint", "NextRXDAT", *nodeType);
            denial = snJoint.NextRXDAT(global, static_cast<std::uint64_t>(decodedRecord.timestamp), flit, &xaction);
        }
    }

    return finishEbXactionProcess(global, denial, xaction, nextOrdinal, std::move(path));
}

template<typename Config>
EbXactionProcessResult<Config> processEbSnpXactionByRecordNode(
    CHI::Eb::Xact::Global<Config>& global,
    CHI::Eb::Xact::Topology& topology,
    CHI::Eb::Xact::RNFJoint<Config>& rnJoint,
    CHI::Eb::Xact::SNFJoint<Config>& snJoint,
    const std::unordered_map<quint16, CLog::NodeType>& nodeTypes,
    const DecodedEbRecord& decodedRecord,
    const CHI::Eb::Flits::SNP<Config>& flit,
    std::uint64_t& nextOrdinal)
{
    const std::optional<CLog::NodeType> nodeType =
        ebXactionNodeTypeForRecord(nodeTypes, decodedRecord);
    if (!nodeType) {
        return finishEbXactionNotIndexed<Config>(
            QStringLiteral("not processed (record nodeId is not listed in topology)"));
    }

    ensureEndpointNode(topology, static_cast<std::uint64_t>(flit.SrcID()), CHI::Eb::Xact::NodeType::HN_F);

    EbXactionPtr<Config> xaction;
    CHI::Eb::Xact::XactDenialEnum denial = nullptr;
    QString path;
    const typename CHI::Eb::Flits::REQ<Config>::srcid_t snpTargetId =
        static_cast<typename CHI::Eb::Flits::REQ<Config>::srcid_t>(
            decodedRecord.direction == FlitDirection::Rx
                ? static_cast<std::uint64_t>(decodedRecord.nodeId)
                : static_cast<std::uint64_t>(flit.FwdNID()));

    if (ebXactionUseRnfJoint(*nodeType)) {
        if (decodedRecord.direction == FlitDirection::Tx) {
            path = ebXactionJointPath("RNFJoint", "NextTXSNP", *nodeType);
            denial = rnJoint.NextTXSNP(global,
                                       static_cast<std::uint64_t>(decodedRecord.timestamp),
                                       flit,
                                       snpTargetId,
                                       &xaction);
        } else {
            path = ebXactionJointPath("RNFJoint", "NextRXSNP", *nodeType);
            denial = rnJoint.NextRXSNP(global,
                                       static_cast<std::uint64_t>(decodedRecord.timestamp),
                                       flit,
                                       snpTargetId,
                                       &xaction);
        }
    } else if (ebXactionUseSnfJoint(*nodeType)) {
        if (decodedRecord.direction == FlitDirection::Tx) {
            path = ebXactionJointPath("SNFJoint", "NextTXSNP", *nodeType);
            denial = snJoint.NextTXSNP(global,
                                       static_cast<std::uint64_t>(decodedRecord.timestamp),
                                       flit,
                                       snpTargetId,
                                       &xaction);
        } else {
            path = ebXactionJointPath("SNFJoint", "NextRXSNP", *nodeType);
            denial = snJoint.NextRXSNP(global,
                                       static_cast<std::uint64_t>(decodedRecord.timestamp),
                                       flit,
                                       snpTargetId,
                                       &xaction);
        }
    }

    return finishEbXactionProcess(global, denial, xaction, nextOrdinal, std::move(path));
}

constexpr size_t byteAlignedBitLength(const size_t bitLength)
{
    return ((bitLength + 7U) / 8U) * 8U;
}

std::uint64_t streamBytePosition(std::istream& stream, const std::uint64_t fallback)
{
    const std::streampos position = stream.tellg();
    if (position < 0) {
        return fallback;
    }

    return static_cast<std::uint64_t>(position);
}

void reportProgress(const CLogBTraceLoadProgressCallback& progressCallback,
                    const std::uint64_t processedBytes,
                    const std::uint64_t totalBytes)
{
    if (progressCallback) {
        progressCallback(processedBytes, totalBytes);
    }
}

void reportStage(const CLogBTraceLoadStageCallback& stageCallback,
                 const CLogBTraceLoadStage stage,
                 const std::size_t workerCount,
                 const std::size_t totalRecords)
{
    if (stageCallback) {
        stageCallback(stage, workerCount, totalRecords);
    }
}

void reportTagDecodeProgress(const CLogBTraceLoadProgressCallback& progressCallback,
                             const std::uint64_t progressStartBytes,
                             const std::uint64_t progressEndBytes,
                             const std::uint64_t totalBytes,
                             const std::size_t completedRecords,
                             const std::size_t totalRecords)
{
    if (!progressCallback) {
        return;
    }

    if (totalRecords == 0) {
        progressCallback(progressEndBytes, totalBytes);
        return;
    }

    const std::uint64_t spanBytes = progressEndBytes >= progressStartBytes
        ? (progressEndBytes - progressStartBytes)
        : 0U;
    const std::uint64_t processedBytes = progressStartBytes
        + (spanBytes * static_cast<std::uint64_t>(completedRecords))
            / static_cast<std::uint64_t>(totalRecords);

    progressCallback(processedBytes, totalBytes);
}

std::size_t suggestedDecodeWorkerCount(const std::size_t recordCount,
                                       const bool allowParallelDecode = true,
                                       const std::size_t maxDecodeWorkers = 4)
{
    if (!allowParallelDecode) {
        return 1;
    }

    const unsigned int requestedWorkers = static_cast<unsigned int>(
        std::max<std::size_t>(1, maxDecodeWorkers));
    const unsigned int hardwareWorkers = std::min(
        requestedWorkers,
        std::max(1U, std::thread::hardware_concurrency()));
    if (recordCount < 2048 || hardwareWorkers <= 1U) {
        return 1;
    }

    const std::size_t chunkLimitedWorkers = std::max<std::size_t>(1, recordCount / 1024);
    return std::min<std::size_t>(hardwareWorkers, chunkLimitedWorkers);
}

template<typename TmatchFn>
void collectMatchingLocalRows(const std::size_t begin,
                              const std::size_t end,
                              TmatchFn&& matchFn,
                              std::vector<std::size_t>& localRows)
{
    localRows.clear();
    if (end <= begin) {
        return;
    }

    const std::size_t recordCount = end - begin;
    const std::size_t workerCount = Detail::suggestedFilterWorkerCount(recordCount);
    if (workerCount <= 1) {
        localRows.reserve(recordCount);
        for (std::size_t localIndex = begin; localIndex < end; ++localIndex) {
            if (matchFn(localIndex)) {
                localRows.push_back(localIndex);
            }
        }
        return;
    }

    std::vector<std::vector<std::size_t>> workerMatches(workerCount);
    std::vector<std::jthread> workers;
    workers.reserve(workerCount);

    std::atomic_bool failed = false;
    std::exception_ptr workerException;
    std::mutex workerExceptionMutex;

    const std::size_t recordsPerWorker = (recordCount + workerCount - 1) / workerCount;
    std::size_t activeWorkers = 0;
    for (std::size_t workerIndex = 0; workerIndex < workerCount; ++workerIndex) {
        const std::size_t workerBegin = begin + workerIndex * recordsPerWorker;
        if (workerBegin >= end) {
            break;
        }

        const std::size_t workerEnd = std::min(end, workerBegin + recordsPerWorker);
        workers.emplace_back([&, workerIndex, workerBegin, workerEnd]() {
            try {
                std::vector<std::size_t>& matches = workerMatches[workerIndex];
                matches.reserve(workerEnd - workerBegin);
                for (std::size_t localIndex = workerBegin; localIndex < workerEnd; ++localIndex) {
                    if (failed.load(std::memory_order_relaxed)) {
                        return;
                    }

                    if (matchFn(localIndex)) {
                        matches.push_back(localIndex);
                    }
                }
            } catch (...) {
                failed.store(true, std::memory_order_relaxed);
                std::lock_guard<std::mutex> lock(workerExceptionMutex);
                if (!workerException) {
                    workerException = std::current_exception();
                }
            }
        });
        ++activeWorkers;
    }

    workers.clear();
    if (workerException) {
        std::rethrow_exception(workerException);
    }

    std::size_t totalMatchedRows = 0;
    for (std::size_t workerIndex = 0; workerIndex < activeWorkers; ++workerIndex) {
        totalMatchedRows += workerMatches[workerIndex].size();
    }

    localRows.reserve(totalMatchedRows);
    for (std::size_t workerIndex = 0; workerIndex < activeWorkers; ++workerIndex) {
        localRows.insert(localRows.end(),
                         workerMatches[workerIndex].begin(),
                         workerMatches[workerIndex].end());
    }
}

std::size_t suggestedProgressBatchSize(const std::size_t recordCount)
{
    constexpr std::size_t kMinBatchSize = 32;
    constexpr std::size_t kTargetUpdates = 256;

    if (recordCount <= kMinBatchSize) {
        return 1;
    }

    return std::max<std::size_t>(kMinBatchSize, recordCount / kTargetUpdates);
}

constexpr std::size_t recordStorageBytes(const CLog::CLogB::TagCHIRecords::Record& record)
{
    return 8U + static_cast<std::size_t>(record.flitLength);
}

bool isValidChannel(const CLog::Channel channel) noexcept
{
    switch (channel) {
    case CLog::Channel::TXREQ:
    case CLog::Channel::TXRSP:
    case CLog::Channel::TXDAT:
    case CLog::Channel::TXSNP:
    case CLog::Channel::RXREQ:
    case CLog::Channel::RXRSP:
    case CLog::Channel::RXDAT:
    case CLog::Channel::RXSNP:
    case CLog::Channel::TXREQ_BeforeSAM:
    case CLog::Channel::RXREQ_BeforeSAM:
        return true;
    }

    return false;
}

bool isTxChannel(const CLog::Channel channel) noexcept
{
    return channel == CLog::Channel::TXREQ
        || channel == CLog::Channel::TXRSP
        || channel == CLog::Channel::TXDAT
        || channel == CLog::Channel::TXSNP
        || channel == CLog::Channel::TXREQ_BeforeSAM;
}

bool isBeforeSamChannel(const CLog::Channel channel) noexcept
{
    return channel == CLog::Channel::TXREQ_BeforeSAM
        || channel == CLog::Channel::RXREQ_BeforeSAM;
}

void applyBeforeSamMarker(FlitRecord& row)
{
    row.channelTag = QStringLiteral("Before SAM");
    row.dimTarget = true;
}

std::size_t flitChannelIndex(const CLog::Channel channel)
{
    switch (channel) {
    case CLog::Channel::TXREQ:
    case CLog::Channel::RXREQ:
    case CLog::Channel::TXREQ_BeforeSAM:
    case CLog::Channel::RXREQ_BeforeSAM:
        return 0;
    case CLog::Channel::TXRSP:
    case CLog::Channel::RXRSP:
        return 1;
    case CLog::Channel::TXDAT:
    case CLog::Channel::RXDAT:
        return 2;
    case CLog::Channel::TXSNP:
    case CLog::Channel::RXSNP:
        return 3;
    }

    return 0;
}

void setFlitLengthMask(CLogBTraceFlitLengthMask& mask, const std::uint8_t byteLength) noexcept
{
    const std::size_t wordIndex = static_cast<std::size_t>(byteLength / 64U);
    const std::size_t bitIndex = static_cast<std::size_t>(byteLength % 64U);
    if (wordIndex < mask.size()) {
        mask[wordIndex] |= (std::uint64_t{1} << bitIndex);
    }
}

void mergeFlitLengthMask(CLogBTraceFlitLengthMask& destination,
                         const CLogBTraceFlitLengthMask& source) noexcept
{
    for (std::size_t index = 0; index < destination.size(); ++index) {
        destination[index] |= source[index];
    }
}

bool flitLengthMaskIsEmpty(const CLogBTraceFlitLengthMask& mask) noexcept
{
    return std::all_of(mask.begin(), mask.end(), [](const std::uint64_t word) {
        return word == 0;
    });
}

bool flitLengthMaskMatchesOnly(const CLogBTraceFlitLengthMask& mask,
                               const std::uint8_t byteLength) noexcept
{
    if (flitLengthMaskIsEmpty(mask)) {
        return true;
    }

    CLogBTraceFlitLengthMask expected{};
    setFlitLengthMask(expected, byteLength);
    return mask == expected;
}

std::vector<std::uint8_t> flitLengthMaskValues(const CLogBTraceFlitLengthMask& mask)
{
    std::vector<std::uint8_t> values;
    for (std::size_t byteLength = 0; byteLength < 256; ++byteLength) {
        const std::size_t wordIndex = byteLength / 64U;
        const std::size_t bitIndex = byteLength % 64U;
        if ((mask[wordIndex] & (std::uint64_t{1} << bitIndex)) != 0) {
            values.push_back(static_cast<std::uint8_t>(byteLength));
        }
    }

    return values;
}

constexpr std::uint64_t kCLogBTagHeaderBytes = 9U;
constexpr std::uint64_t kCLogBRecordBlockHeaderBytes = 20U;
constexpr std::size_t kMetadataScanChunkBytes = 256U << 10;
constexpr qulonglong kMaxGuiTimestampValue =
    static_cast<qulonglong>(std::numeric_limits<qint64>::max());

bool advanceRecordTimestamp(qulonglong& runningTimestamp,
                            const std::uint32_t timeShift) noexcept
{
    if (runningTimestamp > kMaxGuiTimestampValue) {
        return false;
    }

    const qulonglong delta = static_cast<qulonglong>(timeShift);
    if (delta > (kMaxGuiTimestampValue - runningTimestamp)) {
        return false;
    }

    runningTimestamp += delta;
    return true;
}

struct RawTagHeader {
    std::uint8_t type = CLog::CLogB::Encodings::_EOF;
    std::uint64_t payloadLength = 0;
    std::uint64_t fileOffset = 0;
    std::uint64_t payloadFileOffset = 0;
};

class RecordBlockScanner final {
public:
    RecordBlockScanner(const std::uint32_t expectedRecordCount,
                       const std::uint64_t timeBase) noexcept
        : expectedRecordCount_(expectedRecordCount)
        , timeBase_(timeBase)
        , currentTimestamp_(static_cast<qulonglong>(timeBase))
    {
    }

    bool consume(const unsigned char* data, const std::size_t size, std::string& errorMessage)
    {
        std::size_t index = 0;
        while (index < size) {
            switch (state_) {
            case State::TimeShift:
                if (!consumeField(data, size, index, 4)) {
                    continue;
                }
                currentTimeShift_ = std::uint32_t(fieldBuffer_[0])
                                  | (std::uint32_t(fieldBuffer_[1]) << 8)
                                  | (std::uint32_t(fieldBuffer_[2]) << 16)
                                  | (std::uint32_t(fieldBuffer_[3]) << 24);
                fieldBytes_ = 0;
                state_ = State::NodeId;
                break;

            case State::NodeId:
                if (!consumeField(data, size, index, 2)) {
                    continue;
                }
                fieldBytes_ = 0;
                state_ = State::Channel;
                break;

            case State::Channel:
                if (!consumeField(data, size, index, 1)) {
                    continue;
                }
                currentChannel_ = fieldBuffer_[0];
                if (!isValidChannel(static_cast<CLog::Channel>(currentChannel_))) {
                    errorMessage = "CLog.B metadata scan: record block contains an invalid channel value.";
                    return false;
                }
                fieldBytes_ = 0;
                state_ = State::FlitLength;
                break;

            case State::FlitLength:
                if (!consumeField(data, size, index, 1)) {
                    continue;
                }
                currentFlitLength_ = fieldBuffer_[0];
                fieldBytes_ = 0;
                if (!commitRecordHeader(errorMessage)) {
                    return false;
                }
                payloadBytesRemaining_ = currentFlitLength_;
                if (payloadBytesRemaining_ == 0) {
                    finishRecord();
                } else {
                    state_ = State::FlitPayload;
                }
                break;

            case State::FlitPayload: {
                const std::size_t chunkBytes = std::min(size - index, payloadBytesRemaining_);
                index += chunkBytes;
                payloadBytesRemaining_ -= chunkBytes;
                if (payloadBytesRemaining_ == 0) {
                    finishRecord();
                }
                break;
            }
            }
        }

        return true;
    }

    bool finish(std::string& errorMessage) const
    {
        if (state_ != State::TimeShift || fieldBytes_ != 0 || payloadBytesRemaining_ != 0) {
            errorMessage = "CLog.B metadata scan: record block ended in the middle of a record.";
            return false;
        }

        if (parsedRecordCount_ != expectedRecordCount_) {
            errorMessage = "CLog.B metadata scan: record block count does not match its payload.";
            return false;
        }

        return true;
    }

    qint64 firstTimestamp() const noexcept
    {
        return firstTimestamp_;
    }

    qint64 lastTimestamp() const noexcept
    {
        return lastTimestamp_;
    }

    CLogBTraceChannelMask channelMask() const noexcept
    {
        return channelMask_;
    }

    const std::array<std::uint32_t, 4>& channelCounts() const noexcept
    {
        return channelCounts_;
    }

    const CLogBTraceFlitLengthMasks& flitLengthMasks() const noexcept
    {
        return flitLengthMasks_;
    }

private:
    enum class State {
        TimeShift,
        NodeId,
        Channel,
        FlitLength,
        FlitPayload
    };

private:
    bool consumeField(const unsigned char* data,
                      const std::size_t size,
                      std::size_t& index,
                      const std::size_t requiredBytes)
    {
        const std::size_t chunkBytes = std::min(requiredBytes - fieldBytes_, size - index);
        std::copy_n(data + index, chunkBytes, fieldBuffer_.begin() + static_cast<std::ptrdiff_t>(fieldBytes_));
        fieldBytes_ += chunkBytes;
        index += chunkBytes;
        return fieldBytes_ == requiredBytes;
    }

    bool commitRecordHeader(std::string& errorMessage)
    {
        if (parsedRecordCount_ >= expectedRecordCount_) {
            errorMessage = "CLog.B metadata scan: record block contains extra data beyond the declared count.";
            return false;
        }

        if (!advanceRecordTimestamp(currentTimestamp_, currentTimeShift_)) {
            errorMessage = "CLog.B metadata scan: record timestamp exceeds the GUI timestamp range.";
            return false;
        }

        if (parsedRecordCount_ == 0) {
            firstTimestamp_ = static_cast<qint64>(currentTimestamp_);
        }
        lastTimestamp_ = static_cast<qint64>(currentTimestamp_);
        channelMask_ = static_cast<CLogBTraceChannelMask>(
            channelMask_ | (CLogBTraceChannelMask{1} << currentChannel_));
        const std::size_t channelIndex = flitChannelIndex(static_cast<CLog::Channel>(currentChannel_));
        ++channelCounts_[channelIndex];
        setFlitLengthMask(flitLengthMasks_[channelIndex], currentFlitLength_);
        return true;
    }

    void finishRecord() noexcept
    {
        ++parsedRecordCount_;
        state_ = State::TimeShift;
    }

private:
    std::uint32_t expectedRecordCount_ = 0;
    std::uint32_t parsedRecordCount_ = 0;
    std::uint64_t timeBase_ = 0;
    qulonglong currentTimestamp_ = 0;
    State state_ = State::TimeShift;
    std::array<unsigned char, 4> fieldBuffer_{};
    std::size_t fieldBytes_ = 0;
    std::size_t payloadBytesRemaining_ = 0;
    std::uint32_t currentTimeShift_ = 0;
    unsigned char currentChannel_ = 0;
    unsigned char currentFlitLength_ = 0;
    qint64 firstTimestamp_ = 0;
    qint64 lastTimestamp_ = 0;
    CLogBTraceChannelMask channelMask_ = 0;
    std::array<std::uint32_t, 4> channelCounts_{};
    CLogBTraceFlitLengthMasks flitLengthMasks_{};
};

bool readRawTagHeader(std::istream& input,
                      RawTagHeader& header,
                      std::string& errorMessage,
                      const std::uint64_t fallbackOffset)
{
    header.fileOffset = streamBytePosition(input, fallbackOffset);

    if (!input.read(reinterpret_cast<char*>(&header.type), sizeof(header.type))) {
        if (input.eof()) {
            header.type = CLog::CLogB::Encodings::_EOF;
            header.payloadLength = 0;
            header.payloadFileOffset = header.fileOffset;
            return true;
        }

        errorMessage = "CLog.B metadata scan: tag type: unexpected EOF.";
        return false;
    }

    std::uint64_t rawLength = 0;
    if (!input.read(reinterpret_cast<char*>(&rawLength), sizeof(rawLength))) {
        errorMessage = "CLog.B metadata scan: tag length: unexpected EOF.";
        return false;
    }

    header.payloadLength = CLog::CLogB::details::__bswap64_to_little(rawLength);
    header.payloadFileOffset = streamBytePosition(input, header.fileOffset + kCLogBTagHeaderBytes);
    return true;
}

bool seekStreamAbsolute(std::istream& input,
                        const std::uint64_t offset,
                        std::string& errorMessage)
{
    input.clear();
    input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!input) {
        errorMessage = "CLog.B loader: failed to seek to the requested record block.";
        return false;
    }

    return true;
}

bool accumulateTagTimestampBase(const CLog::CLogB::TagCHIRecords& tag,
                                const std::size_t recordCount,
                                qulonglong& accumulatedBase,
                                std::size_t& failedRecordIndex) noexcept
{
    accumulatedBase = static_cast<qulonglong>(tag.head.timeBase);
    failedRecordIndex = 0;

    if (accumulatedBase > kMaxGuiTimestampValue) {
        return false;
    }

    const std::size_t clampedRecordCount = std::min(recordCount, tag.records.size());
    for (std::size_t index = 0; index < clampedRecordCount; ++index) {
        if (!advanceRecordTimestamp(accumulatedBase, tag.records[index].timeShift)) {
            failedRecordIndex = index;
            return false;
        }
    }

    return true;
}

bool buildRecordTimestamps(const CLog::CLogB::TagCHIRecords& tag,
                           std::vector<qulonglong>& recordTimestamps,
                           std::size_t& failedRecordIndex) noexcept
{
    recordTimestamps.clear();
    recordTimestamps.resize(tag.records.size());

    qulonglong runningTimestamp = 0;
    if (!accumulateTagTimestampBase(tag, 0, runningTimestamp, failedRecordIndex)) {
        return false;
    }

    for (std::size_t index = 0; index < tag.records.size(); ++index) {
        if (!advanceRecordTimestamp(runningTimestamp, tag.records[index].timeShift)) {
            failedRecordIndex = index;
            recordTimestamps.clear();
            return false;
        }

        recordTimestamps[index] = runningTimestamp;
    }

    return true;
}

bool advanceToTagEnd(std::istream& input,
                     const RawTagHeader& header,
                     std::string& errorMessage)
{
    const std::uint64_t currentOffset = streamBytePosition(input, header.payloadFileOffset);
    const std::uint64_t tagEndOffset = header.payloadFileOffset + header.payloadLength;
    if (currentOffset > tagEndOffset) {
        errorMessage = "CLog.B metadata scan: tag payload exceeded its declared length.";
        return false;
    }

    if (currentOffset < tagEndOffset
        && !seekStreamAbsolute(input, tagEndOffset, errorMessage)) {
        return false;
    }

    return true;
}

bool scanRecordBlock(std::istream& input,
                     const RawTagHeader& header,
                     CLogBTraceBlockSummary& block,
                     std::string& errorMessage,
                     const CLogBTraceLoadCallbacks& callbacks,
                     const std::uint64_t totalBytes,
                     std::stop_token stopToken = {})
{
    std::uint32_t rawRecordCount = 0;
    if (!input.read(reinterpret_cast<char*>(&rawRecordCount), sizeof(rawRecordCount))) {
        errorMessage = "CLog.B metadata scan: CHI_RECORDS count: unexpected EOF.";
        return false;
    }
    block.recordCount = CLog::CLogB::details::__bswap32_to_little(rawRecordCount);

    std::uint64_t rawTimeBase = 0;
    if (!input.read(reinterpret_cast<char*>(&rawTimeBase), sizeof(rawTimeBase))) {
        errorMessage = "CLog.B metadata scan: CHI_RECORDS timeBase: unexpected EOF.";
        return false;
    }
    const std::uint64_t timeBase = CLog::CLogB::details::__bswap64_to_little(rawTimeBase);

    std::uint32_t rawUncompressedBytes = 0;
    if (!input.read(reinterpret_cast<char*>(&rawUncompressedBytes), sizeof(rawUncompressedBytes))) {
        errorMessage = "CLog.B metadata scan: CHI_RECORDS length: unexpected EOF.";
        return false;
    }
    block.uncompressedBytes = CLog::CLogB::details::__bswap32_to_little(rawUncompressedBytes);

    std::uint32_t rawCompressedBytes = 0;
    if (!input.read(reinterpret_cast<char*>(&rawCompressedBytes), sizeof(rawCompressedBytes))) {
        errorMessage = "CLog.B metadata scan: CHI_RECORDS compressed length: unexpected EOF.";
        return false;
    }
    block.compressedBytes = CLog::CLogB::details::__bswap32_to_little(rawCompressedBytes);
    block.payloadFileOffset = streamBytePosition(
        input,
        header.payloadFileOffset + kCLogBRecordBlockHeaderBytes);

    const std::uint64_t payloadBytes = block.compressedBytes != 0
        ? static_cast<std::uint64_t>(block.compressedBytes)
        : static_cast<std::uint64_t>(block.uncompressedBytes);
    if (header.payloadLength != kCLogBRecordBlockHeaderBytes + payloadBytes) {
        errorMessage = "CLog.B metadata scan: CHI_RECORDS tag length does not match its payload header.";
        return false;
    }

    RecordBlockScanner scanner(block.recordCount, timeBase);
    std::array<unsigned char, kMetadataScanChunkBytes> inputChunk{};

    if (block.compressedBytes == 0) {
        std::uint64_t remainingBytes = payloadBytes;
        while (remainingBytes > 0) {
            if (stopToken.stop_requested()) {
                errorMessage = "cancelled";
                return false;
            }

            const std::size_t chunkBytes = static_cast<std::size_t>(std::min<std::uint64_t>(
                remainingBytes,
                inputChunk.size()));
            if (!input.read(reinterpret_cast<char*>(inputChunk.data()),
                            static_cast<std::streamsize>(chunkBytes))) {
                errorMessage = "CLog.B metadata scan: CHI_RECORDS payload: unexpected EOF.";
                return false;
            }

            if (!scanner.consume(inputChunk.data(), chunkBytes, errorMessage)) {
                return false;
            }

            remainingBytes -= chunkBytes;
            reportProgress(callbacks.progress,
                           streamBytePosition(input, header.payloadFileOffset),
                           totalBytes);
        }
    } else {
        std::array<unsigned char, kMetadataScanChunkBytes> outputChunk{};
        std::uint64_t remainingCompressedBytes = payloadBytes;
        z_stream inflateStream{};
        inflateStream.zalloc = Z_NULL;
        inflateStream.zfree = Z_NULL;
        inflateStream.opaque = Z_NULL;

        const int initStatus = inflateInit(&inflateStream);
        if (initStatus != Z_OK) {
            errorMessage = "CLog.B metadata scan: failed to initialize zlib for CHI_RECORDS.";
            return false;
        }

        bool ok = true;
        while (ok) {
            if (stopToken.stop_requested()) {
                errorMessage = "cancelled";
                ok = false;
                break;
            }

            if (inflateStream.avail_in == 0 && remainingCompressedBytes > 0) {
                const std::size_t chunkBytes = static_cast<std::size_t>(std::min<std::uint64_t>(
                    remainingCompressedBytes,
                    inputChunk.size()));
                if (!input.read(reinterpret_cast<char*>(inputChunk.data()),
                                static_cast<std::streamsize>(chunkBytes))) {
                    errorMessage = "CLog.B metadata scan: compressed CHI_RECORDS payload: unexpected EOF.";
                    ok = false;
                    break;
                }

                remainingCompressedBytes -= chunkBytes;
                inflateStream.next_in = inputChunk.data();
                inflateStream.avail_in = static_cast<uInt>(chunkBytes);
                reportProgress(callbacks.progress,
                               streamBytePosition(input, header.payloadFileOffset),
                               totalBytes);
            }

            inflateStream.next_out = outputChunk.data();
            inflateStream.avail_out = static_cast<uInt>(outputChunk.size());
            const uLong totalInBefore = inflateStream.total_in;
            const uLong totalOutBefore = inflateStream.total_out;
            const int inflateStatus = inflate(&inflateStream, Z_NO_FLUSH);
            const std::size_t producedBytes = static_cast<std::size_t>(inflateStream.total_out - totalOutBefore);
            const std::size_t consumedBytes = static_cast<std::size_t>(inflateStream.total_in - totalInBefore);

            if (producedBytes > 0
                && !scanner.consume(outputChunk.data(), producedBytes, errorMessage)) {
                ok = false;
                break;
            }

            if (inflateStatus == Z_STREAM_END) {
                if (remainingCompressedBytes != 0 || inflateStream.avail_in != 0) {
                    errorMessage = "CLog.B metadata scan: compressed CHI_RECORDS block contains trailing bytes.";
                    ok = false;
                }
                break;
            }

            if (inflateStatus != Z_OK && inflateStatus != Z_BUF_ERROR) {
                errorMessage = "CLog.B metadata scan: zlib inflate failed for a CHI_RECORDS block.";
                ok = false;
                break;
            }

            if (remainingCompressedBytes == 0 && inflateStream.avail_in == 0
                && producedBytes == 0 && consumedBytes == 0) {
                errorMessage = "CLog.B metadata scan: compressed CHI_RECORDS block ended before reaching the end of the stream.";
                ok = false;
                break;
            }
        }

        inflateEnd(&inflateStream);

        if (!ok) {
            return false;
        }

        if (inflateStream.total_out != static_cast<uLong>(block.uncompressedBytes)) {
            errorMessage = "CLog.B metadata scan: compressed CHI_RECORDS block expanded to an unexpected size.";
            return false;
        }
    }

    if (!scanner.finish(errorMessage)) {
        return false;
    }

    block.firstTimestamp = scanner.firstTimestamp();
    block.lastTimestamp = scanner.lastTimestamp();
    block.channelMask = scanner.channelMask();
    block.channelCounts = scanner.channelCounts();
    block.flitLengthMasks = scanner.flitLengthMasks();
    return true;
}

template<typename TdecodeFn>
bool decodeRecords(const std::size_t recordCount,
                   TdecodeFn&& decodeFn,
                   CLogBTraceLoadError& error,
                   const DecodeProgressCallback& progressCallback = {},
                   const CLogBTraceLoadWorkerProgressCallback& workerProgressCallback = {},
                   std::stop_token stopToken = {},
                   const bool allowParallelDecode = true,
                   const std::size_t maxDecodeWorkers = 4)
{
    const std::size_t workerCount =
        suggestedDecodeWorkerCount(recordCount, allowParallelDecode, maxDecodeWorkers);
    const auto reportProgress = [&](const std::size_t completedRecords) {
        if (progressCallback) {
            progressCallback(completedRecords, recordCount);
        }
    };

    if (stopToken.stop_requested()) {
        setCancelledLoadError(error);
        return false;
    }

    if (workerCount <= 1) {
        const std::size_t progressBatchSize = suggestedProgressBatchSize(recordCount);
        std::size_t completedRecords = 0;

        if (workerProgressCallback) {
            workerProgressCallback(CLogBTraceLoadWorkerProgress{
                .workerIndex = 0,
                .completedRecords = 0,
                .totalRecords = recordCount,
            });
        }

        for (std::size_t index = 0; index < recordCount; ++index) {
            if (stopToken.stop_requested()) {
                setCancelledLoadError(error);
                return false;
            }

            if (!decodeFn(index, error)) {
                return false;
            }

            ++completedRecords;
            if (workerProgressCallback) {
                workerProgressCallback(CLogBTraceLoadWorkerProgress{
                    .workerIndex = 0,
                    .completedRecords = completedRecords,
                    .totalRecords = recordCount,
                });
            }
            if (progressCallback
                && ((completedRecords % progressBatchSize) == 0 || completedRecords == recordCount)) {
                reportProgress(completedRecords);
            }
        }

        return true;
    }

    std::atomic_bool failed = false;
    std::vector<std::jthread> workers;
    workers.reserve(workerCount);

    const std::size_t progressBatchSize = suggestedProgressBatchSize(recordCount);
    std::atomic<std::size_t> completedRecords = 0;
    std::atomic<std::size_t> reportedRecords = 0;

    const std::size_t recordsPerWorker = (recordCount + workerCount - 1) / workerCount;
    for (std::size_t workerIndex = 0; workerIndex < workerCount; ++workerIndex) {
        const std::size_t begin = workerIndex * recordsPerWorker;
        if (begin >= recordCount) {
            break;
        }

        const std::size_t end = std::min(recordCount, begin + recordsPerWorker);
        if (workerProgressCallback) {
            workerProgressCallback(CLogBTraceLoadWorkerProgress{
                .workerIndex = workerIndex,
                .completedRecords = 0,
                .totalRecords = end - begin,
            });
        }
        workers.emplace_back([&, workerIndex, begin, end]() {
            try {
                CLogBTraceLoadError localError;
                std::size_t localCompletedRecords = 0;
                std::size_t workerCompletedRecords = 0;
                const auto flushCompletedRecords = [&]() {
                    if (localCompletedRecords == 0) {
                        return;
                    }

                    workerCompletedRecords += localCompletedRecords;
                    const std::size_t completed = completedRecords.fetch_add(localCompletedRecords, std::memory_order_relaxed)
                        + localCompletedRecords;
                    localCompletedRecords = 0;

                    if (workerProgressCallback) {
                        workerProgressCallback(CLogBTraceLoadWorkerProgress{
                            .workerIndex = workerIndex,
                            .completedRecords = workerCompletedRecords,
                            .totalRecords = end - begin,
                        });
                    }

                    if (!progressCallback) {
                        return;
                    }

                    if ((completed % progressBatchSize) != 0 && completed != recordCount) {
                        return;
                    }

                    std::size_t previousReported = reportedRecords.load(std::memory_order_relaxed);
                    while (completed > previousReported) {
                        if (reportedRecords.compare_exchange_weak(previousReported,
                                                                  completed,
                                                                  std::memory_order_relaxed,
                                                                  std::memory_order_relaxed)) {
                            reportProgress(completed);
                            break;
                        }
                    }
                };

                for (std::size_t index = begin; index < end; ++index) {
                    if (failed.load(std::memory_order_relaxed) || stopToken.stop_requested()) {
                        if (stopToken.stop_requested()) {
                            bool expected = false;
                            if (failed.compare_exchange_strong(expected,
                                                               true,
                                                               std::memory_order_relaxed,
                                                               std::memory_order_relaxed)) {
                                setCancelledLoadError(error);
                            }
                        }
                        return;
                    }

                    if (!decodeFn(index, localError)) {
                        bool expected = false;
                        if (failed.compare_exchange_strong(expected,
                                                           true,
                                                           std::memory_order_relaxed,
                                                           std::memory_order_relaxed)) {
                            error = std::move(localError);
                        }
                        return;
                    }

                    ++localCompletedRecords;
                    if (workerProgressCallback) {
                        workerProgressCallback(CLogBTraceLoadWorkerProgress{
                            .workerIndex = workerIndex,
                            .completedRecords = workerCompletedRecords + localCompletedRecords,
                            .totalRecords = end - begin,
                        });
                    }
                    if (localCompletedRecords >= progressBatchSize) {
                        flushCompletedRecords();
                    }
                }

                flushCompletedRecords();
            } catch (const std::bad_alloc&) {
                bool expected = false;
                if (failed.compare_exchange_strong(expected,
                                                   true,
                                                   std::memory_order_relaxed,
                                                   std::memory_order_relaxed)) {
                    setLoadError(error,
                                 CLogBTraceLoadError::Type::Generic,
                                 QStringLiteral("Decoding ran out of memory."));
                }
            } catch (const std::exception& exception) {
                bool expected = false;
                if (failed.compare_exchange_strong(expected,
                                                   true,
                                                   std::memory_order_relaxed,
                                                   std::memory_order_relaxed)) {
                    setLoadError(error,
                                 CLogBTraceLoadError::Type::Generic,
                                 QStringLiteral("Decoding failed unexpectedly."),
                                 QString::fromLocal8Bit(exception.what()));
                }
            } catch (...) {
                bool expected = false;
                if (failed.compare_exchange_strong(expected,
                                                   true,
                                                   std::memory_order_relaxed,
                                                   std::memory_order_relaxed)) {
                    setLoadError(error,
                                 CLogBTraceLoadError::Type::Generic,
                                 QStringLiteral("Decoding failed unexpectedly."),
                                 QStringLiteral("A non-standard exception escaped a decode worker."));
                }
            }
        });
    }

    workers.clear();
    return !failed.load(std::memory_order_relaxed);
}

QString describeParametersBlock(const CLog::Parameters& params)
{
    return QStringLiteral(
               "Issue:          %1\n"
               "NodeIDWidth:    %2\n"
               "ReqAddrWidth:   %3\n"
               "ReqRSVDCWidth:  %4\n"
               "DatRSVDCWidth:  %5\n"
               "DataWidth:      %6\n"
               "DataCheck:      %7\n"
               "Poison:         %8\n"
               "MPAM:           %9")
        .arg(issueName(params.GetIssue()))
        .arg(static_cast<qulonglong>(params.GetNodeIdWidth()))
        .arg(static_cast<qulonglong>(params.GetReqAddrWidth()))
        .arg(static_cast<qulonglong>(params.GetReqRSVDCWidth()))
        .arg(static_cast<qulonglong>(params.GetDatRSVDCWidth()))
        .arg(static_cast<qulonglong>(params.GetDataWidth()))
        .arg(boolName(params.IsDataCheckPresent()))
        .arg(boolName(params.IsPoisonPresent()))
        .arg(boolName(params.IsMPAMPresent()));
}

QString channelName(const CLog::Channel channel)
{
    switch (channel) {
    case CLog::Channel::TXREQ:
        return QStringLiteral("TXREQ");
    case CLog::Channel::TXREQ_BeforeSAM:
        return QStringLiteral("TXREQ_BeforeSAM");
    case CLog::Channel::TXRSP:
        return QStringLiteral("TXRSP");
    case CLog::Channel::TXDAT:
        return QStringLiteral("TXDAT");
    case CLog::Channel::TXSNP:
        return QStringLiteral("TXSNP");
    case CLog::Channel::RXREQ:
        return QStringLiteral("RXREQ");
    case CLog::Channel::RXREQ_BeforeSAM:
        return QStringLiteral("RXREQ_BeforeSAM");
    case CLog::Channel::RXRSP:
        return QStringLiteral("RXRSP");
    case CLog::Channel::RXDAT:
        return QStringLiteral("RXDAT");
    case CLog::Channel::RXSNP:
        return QStringLiteral("RXSNP");
    }

    return QStringLiteral("Unknown(%1)").arg(static_cast<int>(channel));
}

void appendWidthField(QStringList& lines, const QString& name, const size_t width)
{
    lines.append(
        QStringLiteral("  %1 : %2 bits")
            .arg(name.leftJustified(16, QLatin1Char(' ')),
                 QString::number(static_cast<qulonglong>(width))));
}

void attachRawRecordPayload(FlitRecord& row,
                            const CLog::CLogB::TagCHIRecords::Record& source)
{
    row.rawRecordAvailable = true;
    row.rawNodeId = source.nodeId;
    row.rawChannel = static_cast<std::uint8_t>(source.channel);
    row.rawFlitLength = source.flitLength;
    const std::size_t wordCount = (static_cast<std::size_t>(source.flitLength) + 3U) / 4U;
    row.rawFlitWords.assign(source.flit.get(), source.flit.get() + wordCount);
}

QString formatRawFlitBytes(const uint32_t* flitBits, const size_t byteLength)
{
    if (!flitBits || byteLength == 0) {
        return QStringLiteral("<empty>");
    }

    const uchar* bytes = reinterpret_cast<const uchar*>(flitBits);
    QStringList lines;

    for (size_t offset = 0; offset < byteLength; offset += 16) {
        QStringList fragments;
        const size_t lineEnd = qMin(byteLength, offset + 16);

        for (size_t index = offset; index < lineEnd; ++index) {
            fragments.append(QStringLiteral("%1")
                                 .arg(static_cast<unsigned int>(bytes[index]),
                                      2,
                                      16,
                                      QLatin1Char('0'))
                                 .toUpper());
        }

        lines.append(
            QStringLiteral("%1: %2")
                .arg(QString::number(static_cast<qulonglong>(offset), 16)
                         .toUpper()
                         .rightJustified(4, QLatin1Char('0')),
                     fragments.join(QLatin1Char(' '))));
    }

    return lines.join(QLatin1Char('\n'));
}

QString formatWidthBreakdown(const CHI::Eb::Flits::REQMeasure& measure)
{
    QStringList lines{QStringLiteral("Loader width breakdown (bits):")};
    appendWidthField(lines, QStringLiteral("QoS"), measure.width.QoS);
    appendWidthField(lines, QStringLiteral("TgtID"), measure.width.TgtID);
    appendWidthField(lines, QStringLiteral("SrcID"), measure.width.SrcID);
    appendWidthField(lines, QStringLiteral("TxnID"), measure.width.TxnID);
    appendWidthField(lines, QStringLiteral("ReturnNID"), measure.width.ReturnNID);
    appendWidthField(lines, QStringLiteral("StashNIDValid"), measure.width.StashNIDValid);
    appendWidthField(lines, QStringLiteral("ReturnTxnID"), measure.width.ReturnTxnID);
    appendWidthField(lines, QStringLiteral("Opcode"), measure.width.Opcode);
    appendWidthField(lines, QStringLiteral("Size"), measure.width.Size);
    appendWidthField(lines, QStringLiteral("Addr"), measure.width.Addr);
    appendWidthField(lines, QStringLiteral("NS"), measure.width.NS);
    appendWidthField(lines, QStringLiteral("LikelyShared"), measure.width.LikelyShared);
    appendWidthField(lines, QStringLiteral("AllowRetry"), measure.width.AllowRetry);
    appendWidthField(lines, QStringLiteral("Order"), measure.width.Order);
    appendWidthField(lines, QStringLiteral("PCrdType"), measure.width.PCrdType);
    appendWidthField(lines, QStringLiteral("MemAttr"), measure.width.MemAttr);
    appendWidthField(lines, QStringLiteral("SnpAttr"), measure.width.SnpAttr);
    appendWidthField(lines, QStringLiteral("TagGroupID"), measure.width.TagGroupID);
    appendWidthField(lines, QStringLiteral("Excl"), measure.width.Excl);
    appendWidthField(lines, QStringLiteral("ExpCompAck"), measure.width.ExpCompAck);
    appendWidthField(lines, QStringLiteral("TagOp"), measure.width.TagOp);
    appendWidthField(lines, QStringLiteral("TraceTag"), measure.width.TraceTag);
    appendWidthField(lines, QStringLiteral("MPAM"), measure.width.MPAM);
    appendWidthField(lines, QStringLiteral("RSVDC"), measure.width.RSVDC);
    appendWidthField(lines, QStringLiteral("TOTAL"), measure.width._);
    return lines.join(QLatin1Char('\n'));
}

QString formatWidthBreakdown(const CHI::Eb::Flits::RSPMeasure& measure)
{
    QStringList lines{QStringLiteral("Loader width breakdown (bits):")};
    appendWidthField(lines, QStringLiteral("QoS"), measure.width.QoS);
    appendWidthField(lines, QStringLiteral("TgtID"), measure.width.TgtID);
    appendWidthField(lines, QStringLiteral("SrcID"), measure.width.SrcID);
    appendWidthField(lines, QStringLiteral("TxnID"), measure.width.TxnID);
    appendWidthField(lines, QStringLiteral("Opcode"), measure.width.Opcode);
    appendWidthField(lines, QStringLiteral("RespErr"), measure.width.RespErr);
    appendWidthField(lines, QStringLiteral("Resp"), measure.width.Resp);
    appendWidthField(lines, QStringLiteral("FwdState"), measure.width.FwdState);
    appendWidthField(lines, QStringLiteral("CBusy"), measure.width.CBusy);
    appendWidthField(lines, QStringLiteral("DBID"), measure.width.DBID);
    appendWidthField(lines, QStringLiteral("PCrdType"), measure.width.PCrdType);
    appendWidthField(lines, QStringLiteral("TagOp"), measure.width.TagOp);
    appendWidthField(lines, QStringLiteral("TraceTag"), measure.width.TraceTag);
    appendWidthField(lines, QStringLiteral("TOTAL"), measure.width._);
    return lines.join(QLatin1Char('\n'));
}

QString formatWidthBreakdown(const CHI::Eb::Flits::DATMeasure& measure)
{
    QStringList lines{QStringLiteral("Loader width breakdown (bits):")};
    appendWidthField(lines, QStringLiteral("QoS"), measure.width.QoS);
    appendWidthField(lines, QStringLiteral("TgtID"), measure.width.TgtID);
    appendWidthField(lines, QStringLiteral("SrcID"), measure.width.SrcID);
    appendWidthField(lines, QStringLiteral("TxnID"), measure.width.TxnID);
    appendWidthField(lines, QStringLiteral("HomeNID"), measure.width.HomeNID);
    appendWidthField(lines, QStringLiteral("Opcode"), measure.width.Opcode);
    appendWidthField(lines, QStringLiteral("RespErr"), measure.width.RespErr);
    appendWidthField(lines, QStringLiteral("Resp"), measure.width.Resp);
    appendWidthField(lines, QStringLiteral("DataSource"), measure.width.DataSource);
    appendWidthField(lines, QStringLiteral("CBusy"), measure.width.CBusy);
    appendWidthField(lines, QStringLiteral("DBID"), measure.width.DBID);
    appendWidthField(lines, QStringLiteral("CCID"), measure.width.CCID);
    appendWidthField(lines, QStringLiteral("DataID"), measure.width.DataID);
    appendWidthField(lines, QStringLiteral("TagOp"), measure.width.TagOp);
    appendWidthField(lines, QStringLiteral("Tag"), measure.width.Tag);
    appendWidthField(lines, QStringLiteral("TU"), measure.width.TU);
    appendWidthField(lines, QStringLiteral("TraceTag"), measure.width.TraceTag);
    appendWidthField(lines, QStringLiteral("RSVDC"), measure.width.RSVDC);
    appendWidthField(lines, QStringLiteral("BE"), measure.width.BE);
    appendWidthField(lines, QStringLiteral("Data"), measure.width.Data);
    appendWidthField(lines, QStringLiteral("DataCheck"), measure.width.DataCheck);
    appendWidthField(lines, QStringLiteral("Poison"), measure.width.Poison);
    appendWidthField(lines, QStringLiteral("TOTAL"), measure.width._);
    return lines.join(QLatin1Char('\n'));
}

QString formatWidthBreakdown(const CHI::Eb::Flits::SNPMeasure& measure)
{
    QStringList lines{QStringLiteral("Loader width breakdown (bits):")};
    appendWidthField(lines, QStringLiteral("QoS"), measure.width.QoS);
    appendWidthField(lines, QStringLiteral("SrcID"), measure.width.SrcID);
    appendWidthField(lines, QStringLiteral("TxnID"), measure.width.TxnID);
    appendWidthField(lines, QStringLiteral("FwdNID"), measure.width.FwdNID);
    appendWidthField(lines, QStringLiteral("FwdTxnID"), measure.width.FwdTxnID);
    appendWidthField(lines, QStringLiteral("Opcode"), measure.width.Opcode);
    appendWidthField(lines, QStringLiteral("Addr"), measure.width.Addr);
    appendWidthField(lines, QStringLiteral("NS"), measure.width.NS);
    appendWidthField(lines, QStringLiteral("DoNotGoToSD"), measure.width.DoNotGoToSD);
    appendWidthField(lines, QStringLiteral("RetToSrc"), measure.width.RetToSrc);
    appendWidthField(lines, QStringLiteral("TraceTag"), measure.width.TraceTag);
    appendWidthField(lines, QStringLiteral("MPAM"), measure.width.MPAM);
    appendWidthField(lines, QStringLiteral("TOTAL"), measure.width._);
    return lines.join(QLatin1Char('\n'));
}

QString formatWidthBreakdown(const CHI::B::Flits::REQMeasure& measure)
{
    QStringList lines{QStringLiteral("Loader width breakdown (bits):")};
    appendWidthField(lines, QStringLiteral("QoS"), measure.width.QoS);
    appendWidthField(lines, QStringLiteral("TgtID"), measure.width.TgtID);
    appendWidthField(lines, QStringLiteral("SrcID"), measure.width.SrcID);
    appendWidthField(lines, QStringLiteral("TxnID"), measure.width.TxnID);
    appendWidthField(lines, QStringLiteral("ReturnNID"), measure.width.ReturnNID);
    appendWidthField(lines, QStringLiteral("StashNIDValid"), measure.width.StashNIDValid);
    appendWidthField(lines, QStringLiteral("ReturnTxnID"), measure.width.ReturnTxnID);
    appendWidthField(lines, QStringLiteral("Opcode"), measure.width.Opcode);
    appendWidthField(lines, QStringLiteral("Size"), measure.width.Size);
    appendWidthField(lines, QStringLiteral("Addr"), measure.width.Addr);
    appendWidthField(lines, QStringLiteral("NS"), measure.width.NS);
    appendWidthField(lines, QStringLiteral("LikelyShared"), measure.width.LikelyShared);
    appendWidthField(lines, QStringLiteral("AllowRetry"), measure.width.AllowRetry);
    appendWidthField(lines, QStringLiteral("Order"), measure.width.Order);
    appendWidthField(lines, QStringLiteral("PCrdType"), measure.width.PCrdType);
    appendWidthField(lines, QStringLiteral("MemAttr"), measure.width.MemAttr);
    appendWidthField(lines, QStringLiteral("SnpAttr"), measure.width.SnpAttr);
    appendWidthField(lines, QStringLiteral("LPID"), measure.width.LPID);
    appendWidthField(lines, QStringLiteral("Excl"), measure.width.Excl);
    appendWidthField(lines, QStringLiteral("ExpCompAck"), measure.width.ExpCompAck);
    appendWidthField(lines, QStringLiteral("TraceTag"), measure.width.TraceTag);
    appendWidthField(lines, QStringLiteral("RSVDC"), measure.width.RSVDC);
    appendWidthField(lines, QStringLiteral("TOTAL"), measure.width._);
    return lines.join(QLatin1Char('\n'));
}

QString formatWidthBreakdown(const CHI::B::Flits::RSPMeasure& measure)
{
    QStringList lines{QStringLiteral("Loader width breakdown (bits):")};
    appendWidthField(lines, QStringLiteral("QoS"), measure.width.QoS);
    appendWidthField(lines, QStringLiteral("TgtID"), measure.width.TgtID);
    appendWidthField(lines, QStringLiteral("SrcID"), measure.width.SrcID);
    appendWidthField(lines, QStringLiteral("TxnID"), measure.width.TxnID);
    appendWidthField(lines, QStringLiteral("Opcode"), measure.width.Opcode);
    appendWidthField(lines, QStringLiteral("RespErr"), measure.width.RespErr);
    appendWidthField(lines, QStringLiteral("Resp"), measure.width.Resp);
    appendWidthField(lines, QStringLiteral("FwdState"), measure.width.FwdState);
    appendWidthField(lines, QStringLiteral("DBID"), measure.width.DBID);
    appendWidthField(lines, QStringLiteral("PCrdType"), measure.width.PCrdType);
    appendWidthField(lines, QStringLiteral("TraceTag"), measure.width.TraceTag);
    appendWidthField(lines, QStringLiteral("TOTAL"), measure.width._);
    return lines.join(QLatin1Char('\n'));
}

QString formatWidthBreakdown(const CHI::B::Flits::DATMeasure& measure)
{
    QStringList lines{QStringLiteral("Loader width breakdown (bits):")};
    appendWidthField(lines, QStringLiteral("QoS"), measure.width.QoS);
    appendWidthField(lines, QStringLiteral("TgtID"), measure.width.TgtID);
    appendWidthField(lines, QStringLiteral("SrcID"), measure.width.SrcID);
    appendWidthField(lines, QStringLiteral("TxnID"), measure.width.TxnID);
    appendWidthField(lines, QStringLiteral("HomeNID"), measure.width.HomeNID);
    appendWidthField(lines, QStringLiteral("Opcode"), measure.width.Opcode);
    appendWidthField(lines, QStringLiteral("RespErr"), measure.width.RespErr);
    appendWidthField(lines, QStringLiteral("Resp"), measure.width.Resp);
    appendWidthField(lines, QStringLiteral("DataSource"), measure.width.DataSource);
    appendWidthField(lines, QStringLiteral("DBID"), measure.width.DBID);
    appendWidthField(lines, QStringLiteral("CCID"), measure.width.CCID);
    appendWidthField(lines, QStringLiteral("DataID"), measure.width.DataID);
    appendWidthField(lines, QStringLiteral("TraceTag"), measure.width.TraceTag);
    appendWidthField(lines, QStringLiteral("RSVDC"), measure.width.RSVDC);
    appendWidthField(lines, QStringLiteral("BE"), measure.width.BE);
    appendWidthField(lines, QStringLiteral("Data"), measure.width.Data);
    appendWidthField(lines, QStringLiteral("DataCheck"), measure.width.DataCheck);
    appendWidthField(lines, QStringLiteral("Poison"), measure.width.Poison);
    appendWidthField(lines, QStringLiteral("TOTAL"), measure.width._);
    return lines.join(QLatin1Char('\n'));
}

QString formatWidthBreakdown(const CHI::B::Flits::SNPMeasure& measure)
{
    QStringList lines{QStringLiteral("Loader width breakdown (bits):")};
    appendWidthField(lines, QStringLiteral("QoS"), measure.width.QoS);
    appendWidthField(lines, QStringLiteral("SrcID"), measure.width.SrcID);
    appendWidthField(lines, QStringLiteral("TxnID"), measure.width.TxnID);
    appendWidthField(lines, QStringLiteral("FwdNID"), measure.width.FwdNID);
    appendWidthField(lines, QStringLiteral("FwdTxnID"), measure.width.FwdTxnID);
    appendWidthField(lines, QStringLiteral("Opcode"), measure.width.Opcode);
    appendWidthField(lines, QStringLiteral("Addr"), measure.width.Addr);
    appendWidthField(lines, QStringLiteral("NS"), measure.width.NS);
    appendWidthField(lines, QStringLiteral("DoNotGoToSD"), measure.width.DoNotGoToSD);
    appendWidthField(lines, QStringLiteral("RetToSrc"), measure.width.RetToSrc);
    appendWidthField(lines, QStringLiteral("TraceTag"), measure.width.TraceTag);
    appendWidthField(lines, QStringLiteral("TOTAL"), measure.width._);
    return lines.join(QLatin1Char('\n'));
}

template<typename MeasureT>
QString formatFlitReportSection(const QString& name, const MeasureT& measure)
{
    const size_t logicalWidth = measure.width._;
    const size_t storedBitLength = byteAlignedBitLength(logicalWidth);
    const size_t storedByteLength = storedBitLength / 8U;

    QStringList lines{
        QStringLiteral("%1 Flit").arg(name),
        QStringLiteral("  Logical width : %1 bits").arg(static_cast<qulonglong>(logicalWidth)),
        QStringLiteral("  Stored length : %1 bytes (%2 bits)")
            .arg(static_cast<qulonglong>(storedByteLength))
            .arg(static_cast<qulonglong>(storedBitLength)),
        QString(),
        formatWidthBreakdown(measure),
    };

    return lines.join(QLatin1Char('\n'));
}

QString buildFlitReport(const CLog::Parameters& params, const EbMeasures& measures)
{
    QStringList sections{
        QStringLiteral("Current CHI Flit Report"),
        QStringLiteral("CHI parameters:\n%1").arg(describeParametersBlock(params)),
        formatFlitReportSection(QStringLiteral("REQ"), measures.req),
        formatFlitReportSection(QStringLiteral("RSP"), measures.rsp),
        formatFlitReportSection(QStringLiteral("DAT"), measures.dat),
        formatFlitReportSection(QStringLiteral("SNP"), measures.snp),
    };

    return sections.join(QStringLiteral("\n\n"));
}

QString buildFlitReport(const CLog::Parameters& params, const BMeasures& measures)
{
    QStringList sections{
        QStringLiteral("Current CHI Flit Report"),
        QStringLiteral("CHI parameters:\n%1").arg(describeParametersBlock(params)),
        formatFlitReportSection(QStringLiteral("REQ"), measures.req),
        formatFlitReportSection(QStringLiteral("RSP"), measures.rsp),
        formatFlitReportSection(QStringLiteral("DAT"), measures.dat),
        formatFlitReportSection(QStringLiteral("SNP"), measures.snp),
    };

    return sections.join(QStringLiteral("\n\n"));
}

template<typename MeasureT>
QString buildDeserializeFailureDetails(const CLog::Parameters& params,
                                       const MeasureT& measure,
                                       const CLog::CLogB::TagCHIRecords& tag,
                                       const CLog::CLogB::TagCHIRecords::Record& source,
                                       const std::size_t recordIndex,
                                       const qulonglong timestamp,
                                       const size_t flexibleWidth)
{
    const size_t recordedByteLength = source.flitLength;
    const size_t recordedBitLength = recordedByteLength * 8U;
    const size_t expectedWidth = measure.width._;
    const size_t expectedStoredBitLength = byteAlignedBitLength(expectedWidth);
    const size_t expectedByteLength = expectedStoredBitLength / 8U;
    const qlonglong widthDelta = static_cast<qlonglong>(recordedBitLength)
                               - static_cast<qlonglong>(expectedStoredBitLength);

    QStringList reasonLines{QStringLiteral("Failure checks:")};
    if (recordedBitLength != expectedStoredBitLength) {
        reasonLines.append(QStringLiteral("  Recorded flit length does not match the byte-aligned decoder width."));
    } else {
        reasonLines.append(QStringLiteral("  Recorded flit length matches the byte-aligned decoder width."));
    }
    if (recordedBitLength > flexibleWidth) {
        reasonLines.append(
            QStringLiteral("  Recorded flit length exceeds the compiled flexible-flit capacity."));
    }
    if (recordedByteLength == expectedByteLength && recordedBitLength != expectedWidth) {
        reasonLines.append(
            QStringLiteral("  Recorded byte length matches the rounded-on-disk size for the expected width."));
        reasonLines.append(
            QStringLiteral("  CLog.B stores flitLength in bytes, so non-byte-aligned flits appear padded to the next byte."));
    }

    QStringList metadataLines{
        QStringLiteral("Record metadata:"),
        QStringLiteral("  Record index      : %1").arg(static_cast<qulonglong>(recordIndex + 1)),
        QStringLiteral("  Issue             : %1").arg(issueName(params.GetIssue())),
        QStringLiteral("  Channel           : %1").arg(channelName(source.channel)),
        QStringLiteral("  Node ID           : %1").arg(source.nodeId),
        QStringLiteral("  Time base         : %1").arg(static_cast<qulonglong>(tag.head.timeBase)),
        QStringLiteral("  Time shift        : %1").arg(static_cast<qulonglong>(source.timeShift)),
        QStringLiteral("  Decoded timestamp : %1").arg(timestamp),
        QStringLiteral("  Recorded length   : %1 bytes (%2 bits)")
            .arg(static_cast<qulonglong>(recordedByteLength))
            .arg(static_cast<qulonglong>(recordedBitLength)),
        QStringLiteral("  Logical width     : %1 bits").arg(static_cast<qulonglong>(expectedWidth)),
        QStringLiteral("  Expected stored   : %1 bytes (%2 bits)")
            .arg(static_cast<qulonglong>(expectedByteLength))
            .arg(static_cast<qulonglong>(expectedStoredBitLength)),
        QStringLiteral("  Width delta       : %1 bits").arg(widthDelta),
        QStringLiteral("  Flexible max      : %1 bits").arg(static_cast<qulonglong>(flexibleWidth)),
    };

    QStringList sections;
    sections.append(reasonLines.join(QLatin1Char('\n')));
    sections.append(metadataLines.join(QLatin1Char('\n')));
    sections.append(QStringLiteral("CHI parameters:\n%1").arg(describeParametersBlock(params)));
    sections.append(formatWidthBreakdown(measure));
    sections.append(
        QStringLiteral("Raw flit bytes (little-endian CLog.B payload):\n%1")
            .arg(formatRawFlitBytes(source.flit.get(), recordedByteLength)));
    return sections.join(QStringLiteral("\n\n"));
}

template<typename MeasureT>
void setDeserializationLoadError(CLogBTraceLoadError& error,
                                 const CLog::Parameters& params,
                                 const QString& issue,
                                 const QString& flitKind,
                                 const CLog::CLogB::TagCHIRecords& tag,
                                 const CLog::CLogB::TagCHIRecords::Record& source,
                                 const std::size_t recordIndex,
                                 const qulonglong timestamp,
                                 const MeasureT& measure,
                                 const size_t flexibleWidth)
{
    const size_t bitLength = static_cast<size_t>(source.flitLength) * 8U;
    const CLogBTraceLoadError::Type errorType =
        bitLength == byteAlignedBitLength(measure.width._)
            ? CLogBTraceLoadError::Type::Generic
            : CLogBTraceLoadError::Type::FlitWidthMismatch;

    setLoadError(
        error,
        errorType,
        QStringLiteral("Unable to decode record %1 as a CHI Issue %2 %3 flit.")
            .arg(static_cast<qulonglong>(recordIndex + 1))
            .arg(issue, flitKind),
        QStringLiteral(
            "The file parameters were accepted, but this flit could not be deserialized.\n"
            "Channel: %1\n"
            "Node: %2\n"
            "Stored length: %3 bytes (%4 bits)\n"
            "Open the debug details to inspect the width breakdown and raw flit bytes.")
            .arg(channelName(source.channel))
            .arg(source.nodeId)
            .arg(static_cast<qulonglong>(source.flitLength))
            .arg(static_cast<qulonglong>(bitLength)),
        buildDeserializeFailureDetails(params,
                                       measure,
                                       tag,
                                       source,
                                       recordIndex,
                                       timestamp,
                                       flexibleWidth));
}

QString captureAnnotationForNode(const quint16 nodeId,
                                 const std::unordered_map<quint16, QString>& nodeAnnotations)
{
    const auto found = nodeAnnotations.find(nodeId);
    if (found == nodeAnnotations.cend()) {
        return InternDisplayString(QStringLiteral("Captured at node %1.").arg(nodeId));
    }

    return found->second;
}

FieldEntry* findField(std::vector<FieldEntry>& fields, const QString& name)
{
    for (FieldEntry& field : fields) {
        if (FieldNameText(field) == name) {
            return &field;
        }
    }

    return nullptr;
}

template<typename DatFlitT>
QString formatDataValue(const DatFlitT& flit, const size_t dataWidth)
{
    const int wordCount = static_cast<int>(dataWidth / 64);
    QString value;
    value.reserve(wordCount * 16);

    for (int index = wordCount - 1; index >= 0; --index) {
        value.append(QString::number(static_cast<qulonglong>(flit.Data()[index]), 16)
                         .toUpper()
                         .rightJustified(16, QLatin1Char('0')));
    }

    return value;
}

template<typename DatFlitT>
QString formatDataRaw(const DatFlitT& flit, const size_t dataWidth)
{
    const int wordCount = static_cast<int>(dataWidth / 64);
    QStringList fragments;
    const int previewCount = qMin(wordCount, 4);

    for (int index = 0; index < previewCount; ++index) {
        fragments.append(QStringLiteral("[%1]=0x%2")
                             .arg(index)
                             .arg(QString::number(static_cast<qulonglong>(flit.Data()[index]), 16)
                                      .toUpper()
                                      .rightJustified(16, QLatin1Char('0'))));
    }

    if (wordCount > previewCount) {
        fragments.append(QStringLiteral("..."));
    }

    return fragments.join(QStringLiteral(", "));
}

int hexDigitsForBits(const size_t bitWidth)
{
    return qMax(1, static_cast<int>((bitWidth + 3) / 4));
}

QString formatFixedHex(const qulonglong value, const size_t bitWidth)
{
    return QStringLiteral("0x%1")
        .arg(QString::number(value, 16).toUpper().rightJustified(hexDigitsForBits(bitWidth), QLatin1Char('0')));
}

QString formatFixedRawIntegral(const qulonglong value, const size_t bitWidth)
{
    return QStringLiteral("%1 (%2)")
        .arg(formatFixedHex(value, bitWidth), QString::number(value));
}

void normalizeAddressDisplay(FlitRecord& record,
                             const QString& fieldName,
                             const QString& formattedAddress)
{
    const QString previousAddress = record.address;
    record.address = formattedAddress;

    if (FieldEntry* addressField = findField(record.fields, fieldName)) {
        addressField->value = formattedAddress;
    }

    if (!previousAddress.isEmpty() && !record.summary.isEmpty()) {
        record.summary.replace(previousAddress, formattedAddress);
    }
}

template<typename DatFlitT>
void normalizeDynamicFields(FlitRecord& record,
                            const DatFlitT* datFlit,
                            const CLog::Parameters& params)
{
    const bool mpamPresent = params.IsMPAMPresent();
    const bool dataCheckPresent = params.IsDataCheckPresent();
    const bool poisonPresent = params.IsPoisonPresent();
    const size_t reqRsvdcWidth = params.GetReqRSVDCWidth();
    const size_t datRsvdcWidth = params.GetDatRSVDCWidth();
    const size_t dataWidth = params.GetDataWidth();
    const size_t dataCheckWidth = dataWidth / 8;
    const size_t poisonWidth = dataWidth / 64;

    auto writeIt = record.fields.begin();
    for (auto readIt = record.fields.begin(); readIt != record.fields.end(); ++readIt) {
        FieldEntry& field = *readIt;
        const QString& name = FieldNameText(field);
        const QString& scope = FieldScopeText(field);

        if ((name == QLatin1String("MPAM") && !mpamPresent)
            || (name == QLatin1String("DataCheck") && !dataCheckPresent)
            || (name == QLatin1String("Poison") && !poisonPresent)
            || (name == QLatin1String("RSVDC")
                && ((scope == QLatin1String("REQ") && reqRsvdcWidth == 0)
                    || (scope == QLatin1String("DAT") && datRsvdcWidth == 0)))) {
            continue;
        }

        if (datFlit) {
            if (name == QLatin1String("BE")) {
                field.value = formatFixedHex(static_cast<qulonglong>(datFlit->BE()), dataWidth / 8);
                field.raw = formatFixedRawIntegral(static_cast<qulonglong>(datFlit->BE()), dataWidth / 8);
            } else if (name == QLatin1String("Data")) {
                field.value = formatDataValue(*datFlit, dataWidth);
                field.raw = formatDataRaw(*datFlit, dataWidth);
            } else if (name == QLatin1String("DataCheck") && dataCheckPresent) {
                field.value = formatFixedHex(static_cast<qulonglong>(datFlit->DataCheck()), dataCheckWidth);
                field.raw = formatFixedRawIntegral(static_cast<qulonglong>(datFlit->DataCheck()), dataCheckWidth);
            } else if (name == QLatin1String("Poison") && poisonPresent) {
                field.value = QString::number(static_cast<qulonglong>(datFlit->Poison()));
                field.raw = formatFixedRawIntegral(static_cast<qulonglong>(datFlit->Poison()), poisonWidth);
            } else if (name == QLatin1String("RSVDC")
                       && scope == QLatin1String("DAT")
                       && datRsvdcWidth > 0) {
                field.value = QString::number(static_cast<qulonglong>(datFlit->RSVDC()));
                field.raw = formatFixedRawIntegral(static_cast<qulonglong>(datFlit->RSVDC()), datRsvdcWidth);
            }
        }

        if (writeIt != readIt) {
            *writeIt = std::move(*readIt);
        }
        ++writeIt;
    }

    record.fields.erase(writeIt, record.fields.end());
}

inline void normalizeDynamicFields(FlitRecord& record, std::nullptr_t, const CLog::Parameters& params)
{
    normalizeDynamicFields<FlexibleDatFlit>(record, static_cast<const FlexibleDatFlit*>(nullptr), params);
}

enum class OptionalFieldId {
    Unknown,
    Addr,
    AllowRetry,
    BE,
    CCID,
    Data,
    DataCheck,
    DataID,
    DataSource,
    DBID,
    DoNotGoToSD,
    Excl,
    ExpCompAck,
    FwdNID,
    FwdState,
    FwdTxnID,
    HomeNID,
    LikelyShared,
    LPID,
    MPAM,
    MemAttr,
    NS,
    Opcode,
    Order,
    PCrdType,
    Poison,
    Resp,
    RespErr,
    RetToSrc,
    ReturnNID,
    ReturnTxnID,
    RSVDC,
    Size,
    SnpAttr,
    SrcID,
    StashNIDValid,
    TgtID,
    TraceTag,
    TxnID
};

OptionalFieldId optionalFieldIdForName(const QString& fieldName)
{
    if (fieldName == QLatin1String("Addr")) return OptionalFieldId::Addr;
    if (fieldName == QLatin1String("AllowRetry")) return OptionalFieldId::AllowRetry;
    if (fieldName == QLatin1String("BE")) return OptionalFieldId::BE;
    if (fieldName == QLatin1String("CCID")) return OptionalFieldId::CCID;
    if (fieldName == QLatin1String("Data")) return OptionalFieldId::Data;
    if (fieldName == QLatin1String("DataCheck")) return OptionalFieldId::DataCheck;
    if (fieldName == QLatin1String("DataID")) return OptionalFieldId::DataID;
    if (fieldName == QLatin1String("DataSource")) return OptionalFieldId::DataSource;
    if (fieldName == QLatin1String("DBID")) return OptionalFieldId::DBID;
    if (fieldName == QLatin1String("DoNotGoToSD")) return OptionalFieldId::DoNotGoToSD;
    if (fieldName == QLatin1String("Excl")) return OptionalFieldId::Excl;
    if (fieldName == QLatin1String("ExpCompAck")) return OptionalFieldId::ExpCompAck;
    if (fieldName == QLatin1String("FwdNID")) return OptionalFieldId::FwdNID;
    if (fieldName == QLatin1String("FwdState")) return OptionalFieldId::FwdState;
    if (fieldName == QLatin1String("FwdTxnID")) return OptionalFieldId::FwdTxnID;
    if (fieldName == QLatin1String("HomeNID")) return OptionalFieldId::HomeNID;
    if (fieldName == QLatin1String("LikelyShared")) return OptionalFieldId::LikelyShared;
    if (fieldName == QLatin1String("LPID")) return OptionalFieldId::LPID;
    if (fieldName == QLatin1String("MPAM")) return OptionalFieldId::MPAM;
    if (fieldName == QLatin1String("MemAttr")) return OptionalFieldId::MemAttr;
    if (fieldName == QLatin1String("NS")) return OptionalFieldId::NS;
    if (fieldName == QLatin1String("Opcode")) return OptionalFieldId::Opcode;
    if (fieldName == QLatin1String("Order")) return OptionalFieldId::Order;
    if (fieldName == QLatin1String("PCrdType")) return OptionalFieldId::PCrdType;
    if (fieldName == QLatin1String("Poison")) return OptionalFieldId::Poison;
    if (fieldName == QLatin1String("Resp")) return OptionalFieldId::Resp;
    if (fieldName == QLatin1String("RespErr")) return OptionalFieldId::RespErr;
    if (fieldName == QLatin1String("RetToSrc")) return OptionalFieldId::RetToSrc;
    if (fieldName == QLatin1String("ReturnNID")) return OptionalFieldId::ReturnNID;
    if (fieldName == QLatin1String("ReturnTxnID")) return OptionalFieldId::ReturnTxnID;
    if (fieldName == QLatin1String("RSVDC")) return OptionalFieldId::RSVDC;
    if (fieldName == QLatin1String("Size")) return OptionalFieldId::Size;
    if (fieldName == QLatin1String("SnpAttr")) return OptionalFieldId::SnpAttr;
    if (fieldName == QLatin1String("SrcID")) return OptionalFieldId::SrcID;
    if (fieldName == QLatin1String("StashNIDValid")) return OptionalFieldId::StashNIDValid;
    if (fieldName == QLatin1String("TgtID")) return OptionalFieldId::TgtID;
    if (fieldName == QLatin1String("TraceTag")) return OptionalFieldId::TraceTag;
    if (fieldName == QLatin1String("TxnID")) return OptionalFieldId::TxnID;
    return OptionalFieldId::Unknown;
}

bool setOptionalFieldValue(CLogBTraceOptionalFieldValue& record,
                           QString value,
                           QString raw)
{
    if (value.isEmpty() && raw.isEmpty()) {
        return false;
    }

    record.present = true;
    record.value = std::move(value);
    record.raw = std::move(raw);
    return true;
}

bool setIntegralOptionalField(CLogBTraceOptionalFieldValue& record,
                              const qulonglong value)
{
    return setOptionalFieldValue(record,
                                 QString::number(value),
                                 Detail::RawIntegral(value));
}

bool setFixedIntegralOptionalField(CLogBTraceOptionalFieldValue& record,
                                   const qulonglong value,
                                   const size_t bitWidth)
{
    return setOptionalFieldValue(record,
                                 QString::number(value),
                                 formatFixedRawIntegral(value, bitWidth));
}

QString formatEbRawIntegral(const qulonglong value)
{
    return QStringLiteral("0x%1 (%2)")
        .arg(QString::number(value, 16).toUpper())
        .arg(QString::number(value));
}

QString formatEbRawValue(const CHI::Eb::Expresso::Flit::Value& value)
{
    if (value.IsIntegral()) {
        return formatEbRawIntegral(static_cast<qulonglong>(value.AsIntegral()));
    }

    if (value.IsVector()) {
        const auto& words = value.AsVector();
        QStringList fragments;
        const std::size_t limit = words.size() < 4 ? words.size() : 4U;

        for (std::size_t index = 0; index < limit; ++index) {
            fragments.append(QStringLiteral("[%1]=0x%2")
                                 .arg(index)
                                 .arg(QString::number(static_cast<qulonglong>(words[index]), 16).toUpper()));
        }
        if (words.size() > limit) {
            fragments.append(QStringLiteral("..."));
        }
        return fragments.join(QStringLiteral(", "));
    }

    return QString();
}

QString formatEbDisplay(const FieldKeyValueMap& map, const FieldKey key)
{
    if (map.find(key) == map.end()) {
        return QString();
    }

    thread_local CHI::Eb::Expresso::Flit::DecodingFormatter<ViewerConfig> formatter;
    return Detail::Simplify(QString::fromStdString(key->Format(formatter, map, "{1}")));
}

bool extractEbMappedOptionalField(const FieldKey key,
                                  const FieldKeyValueMap& map,
                                  CLogBTraceOptionalFieldValue& record)
{
    if (!key) {
        return false;
    }

    const auto valueIt = map.find(key);
    if (valueIt == map.end()) {
        return false;
    }

    if (valueIt->second.IsIntegral()) {
        const qulonglong value = static_cast<qulonglong>(valueIt->second.AsIntegral());
        return setOptionalFieldValue(record,
                                     QString::number(value),
                                     formatEbRawIntegral(value));
    }

    return setOptionalFieldValue(record,
                                 formatEbDisplay(map, key),
                                 formatEbRawValue(valueIt->second));
}

template<typename FlitT>
FieldKeyValueMap buildEbFieldValueMap(const FlitT& flit)
{
    CHI::Eb::Expresso::Flit::Mapper mapper;
    mapper.Map(flit);
    return std::move(mapper.Get());
}

FieldKey findEbFieldKey(const CHI::Eb::Expresso::Flit::KeyIteration& keys,
                        const QString& fieldName)
{
    for (auto it = keys.begin(); it != keys.end(); ++it) {
        const FieldKey key = *it;
        if (fieldName == QLatin1String(key->canonicalName)) {
            return key;
        }
    }

    return nullptr;
}

struct EbOptionalFieldKeys {
    FieldKey req = nullptr;
    FieldKey rsp = nullptr;
    FieldKey dat = nullptr;
    FieldKey snp = nullptr;
};

EbOptionalFieldKeys ebOptionalFieldKeysForName(const QString& fieldName)
{
    return EbOptionalFieldKeys{
        .req = findEbFieldKey(CHI::Eb::Expresso::Flit::Keys::REQ::iteration(), fieldName),
        .rsp = findEbFieldKey(CHI::Eb::Expresso::Flit::Keys::RSP::iteration(), fieldName),
        .dat = findEbFieldKey(CHI::Eb::Expresso::Flit::Keys::DAT::iteration(), fieldName),
        .snp = findEbFieldKey(CHI::Eb::Expresso::Flit::Keys::SNP::iteration(), fieldName),
    };
}

bool hasAnyEbOptionalFieldKey(const EbOptionalFieldKeys& keys) noexcept
{
    return keys.req || keys.rsp || keys.dat || keys.snp;
}

bool extractEbReqOptionalField(const CLog::Parameters& params,
                               const OptionalFieldId fieldId,
                               const FieldKey key,
                               const FlexibleReqFlit& flit,
                               CLogBTraceOptionalFieldValue& record)
{
    switch (fieldId) {
    case OptionalFieldId::Addr:
        return setOptionalFieldValue(record,
                                     formatFixedHex(static_cast<qulonglong>(flit.Addr()),
                                                    params.GetReqAddrWidth()),
                                     formatEbRawIntegral(static_cast<qulonglong>(flit.Addr())));
    case OptionalFieldId::MPAM:
        return params.IsMPAMPresent()
            && setIntegralOptionalField(record, static_cast<qulonglong>(flit.MPAM()));
    case OptionalFieldId::RSVDC:
        return params.GetReqRSVDCWidth() > 0
            && setFixedIntegralOptionalField(record,
                                             static_cast<qulonglong>(flit.RSVDC()),
                                             params.GetReqRSVDCWidth());
    case OptionalFieldId::TraceTag:
        return setOptionalFieldValue(record,
                                     QString::number(static_cast<qulonglong>(flit.TraceTag())),
                                     formatEbRawIntegral(static_cast<qulonglong>(flit.TraceTag())));
    default:
        break;
    }

    const FieldKeyValueMap map = buildEbFieldValueMap(flit);
    return extractEbMappedOptionalField(key, map, record);
}

bool extractEbRspOptionalField(const FieldKey key,
                               const OptionalFieldId fieldId,
                               const FlexibleRspFlit& flit,
                               CLogBTraceOptionalFieldValue& record)
{
    if (fieldId == OptionalFieldId::TraceTag) {
        return setOptionalFieldValue(record,
                                     QString::number(static_cast<qulonglong>(flit.TraceTag())),
                                     formatEbRawIntegral(static_cast<qulonglong>(flit.TraceTag())));
    }

    const FieldKeyValueMap map = buildEbFieldValueMap(flit);
    return extractEbMappedOptionalField(key, map, record);
}

bool extractEbDatOptionalField(const CLog::Parameters& params,
                               const OptionalFieldId fieldId,
                               const FieldKey key,
                               const FlexibleDatFlit& flit,
                               CLogBTraceOptionalFieldValue& record)
{
    const size_t dataWidth = params.GetDataWidth();
    switch (fieldId) {
    case OptionalFieldId::BE:
        return setOptionalFieldValue(record,
                                     formatFixedHex(static_cast<qulonglong>(flit.BE()), dataWidth / 8),
                                     formatFixedRawIntegral(static_cast<qulonglong>(flit.BE()), dataWidth / 8));
    case OptionalFieldId::Data:
        return setOptionalFieldValue(record,
                                     formatDataValue(flit, dataWidth),
                                     formatDataRaw(flit, dataWidth));
    case OptionalFieldId::DataCheck:
        return params.IsDataCheckPresent()
            && setOptionalFieldValue(record,
                                     formatFixedHex(static_cast<qulonglong>(flit.DataCheck()), dataWidth / 8),
                                     formatFixedRawIntegral(static_cast<qulonglong>(flit.DataCheck()),
                                                            dataWidth / 8));
    case OptionalFieldId::Poison:
        return params.IsPoisonPresent()
            && setFixedIntegralOptionalField(record,
                                             static_cast<qulonglong>(flit.Poison()),
                                             dataWidth / 64);
    case OptionalFieldId::RSVDC:
        return params.GetDatRSVDCWidth() > 0
            && setFixedIntegralOptionalField(record,
                                             static_cast<qulonglong>(flit.RSVDC()),
                                             params.GetDatRSVDCWidth());
    case OptionalFieldId::TraceTag:
        return setOptionalFieldValue(record,
                                     QString::number(static_cast<qulonglong>(flit.TraceTag())),
                                     formatEbRawIntegral(static_cast<qulonglong>(flit.TraceTag())));
    default:
        break;
    }

    const FieldKeyValueMap map = buildEbFieldValueMap(flit);
    return extractEbMappedOptionalField(key, map, record);
}

bool extractEbSnpOptionalField(const CLog::Parameters& params,
                               const OptionalFieldId fieldId,
                               const FieldKey key,
                               const FlexibleSnpFlit& flit,
                               CLogBTraceOptionalFieldValue& record)
{
    switch (fieldId) {
    case OptionalFieldId::Addr:
        return setOptionalFieldValue(record,
                                     formatFixedHex(static_cast<qulonglong>(flit.Addr() << 3),
                                                    params.GetReqAddrWidth()),
                                     formatEbRawIntegral(static_cast<qulonglong>(flit.Addr())));
    case OptionalFieldId::MPAM:
        return params.IsMPAMPresent()
            && setIntegralOptionalField(record, static_cast<qulonglong>(flit.MPAM()));
    case OptionalFieldId::TraceTag:
        return setOptionalFieldValue(record,
                                     QString::number(static_cast<qulonglong>(flit.TraceTag())),
                                     formatEbRawIntegral(static_cast<qulonglong>(flit.TraceTag())));
    default:
        break;
    }

    const FieldKeyValueMap map = buildEbFieldValueMap(flit);
    return extractEbMappedOptionalField(key, map, record);
}

bool extractBReqOptionalField(const CLog::Parameters& params,
                              const OptionalFieldId fieldId,
                              const FlexibleBReqFlit& flit,
                              CLogBTraceOptionalFieldValue& record)
{
    switch (fieldId) {
    case OptionalFieldId::Opcode:
        return setOptionalFieldValue(
            record,
            Detail::DecodeOpcode<CHI::B::Opcodes::REQ::Decoder<FlexibleBReqFlit>>(flit.Opcode()),
            Detail::HexValue(static_cast<qulonglong>(flit.Opcode())));
    case OptionalFieldId::SrcID:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.SrcID()));
    case OptionalFieldId::TgtID:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.TgtID()));
    case OptionalFieldId::TxnID:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.TxnID()));
    case OptionalFieldId::ReturnNID:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.ReturnNID()));
    case OptionalFieldId::StashNIDValid:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.StashNIDValid()));
    case OptionalFieldId::ReturnTxnID:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.ReturnTxnID()));
    case OptionalFieldId::Size:
        return setOptionalFieldValue(record,
                                     Detail::EnumName(CHI::B::Sizes::ToEnum(flit.Size())),
                                     Detail::RawIntegral(static_cast<qulonglong>(flit.Size())));
    case OptionalFieldId::Addr:
        return setOptionalFieldValue(record,
                                     formatFixedHex(static_cast<qulonglong>(flit.Addr()),
                                                    params.GetReqAddrWidth()),
                                     Detail::RawIntegral(static_cast<qulonglong>(flit.Addr())));
    case OptionalFieldId::NS:
        return setOptionalFieldValue(record,
                                     Detail::EnumName(CHI::B::NSs::ToEnum(flit.NS())),
                                     Detail::RawIntegral(static_cast<qulonglong>(flit.NS())));
    case OptionalFieldId::LikelyShared:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.LikelyShared()));
    case OptionalFieldId::AllowRetry:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.AllowRetry()));
    case OptionalFieldId::Order:
        return setOptionalFieldValue(record,
                                     Detail::EnumName(CHI::B::Orders::ToEnum(flit.Order())),
                                     Detail::RawIntegral(static_cast<qulonglong>(flit.Order())));
    case OptionalFieldId::PCrdType:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.PCrdType()));
    case OptionalFieldId::MemAttr:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.MemAttr()));
    case OptionalFieldId::SnpAttr:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.SnpAttr()));
    case OptionalFieldId::LPID:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.LPID()));
    case OptionalFieldId::Excl:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.Excl()));
    case OptionalFieldId::ExpCompAck:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.ExpCompAck()));
    case OptionalFieldId::TraceTag:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.TraceTag()));
    case OptionalFieldId::RSVDC:
        return params.GetReqRSVDCWidth() > 0
            && setFixedIntegralOptionalField(record,
                                             static_cast<qulonglong>(flit.RSVDC()),
                                             params.GetReqRSVDCWidth());
    default:
        return false;
    }
}

bool extractBRspOptionalField(const OptionalFieldId fieldId,
                              const FlexibleBRspFlit& flit,
                              CLogBTraceOptionalFieldValue& record)
{
    switch (fieldId) {
    case OptionalFieldId::Opcode:
        return setOptionalFieldValue(
            record,
            Detail::DecodeOpcode<CHI::B::Opcodes::RSP::Decoder<FlexibleBRspFlit>>(flit.Opcode()),
            Detail::HexValue(static_cast<qulonglong>(flit.Opcode())));
    case OptionalFieldId::SrcID:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.SrcID()));
    case OptionalFieldId::TgtID:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.TgtID()));
    case OptionalFieldId::TxnID:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.TxnID()));
    case OptionalFieldId::RespErr:
        return setOptionalFieldValue(record,
                                     Detail::EnumName(CHI::B::RespErrs::ToEnum(flit.RespErr())),
                                     Detail::RawIntegral(static_cast<qulonglong>(flit.RespErr())));
    case OptionalFieldId::Resp:
        return setOptionalFieldValue(record,
                                     Detail::EnumName(CHI::B::Resps::ToEnum(flit.Resp())),
                                     Detail::RawIntegral(static_cast<qulonglong>(flit.Resp())));
    case OptionalFieldId::FwdState:
        return setOptionalFieldValue(
            record,
            Detail::EnumName(CHI::B::FwdStates::ToEnum(
                CHI::B::FwdState(static_cast<unsigned int>(flit.FwdState())))),
            Detail::RawIntegral(static_cast<qulonglong>(flit.FwdState())));
    case OptionalFieldId::DBID:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.DBID()));
    case OptionalFieldId::PCrdType:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.PCrdType()));
    case OptionalFieldId::TraceTag:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.TraceTag()));
    default:
        return false;
    }
}

bool extractBDatOptionalField(const CLog::Parameters& params,
                              const OptionalFieldId fieldId,
                              const FlexibleBDatFlit& flit,
                              CLogBTraceOptionalFieldValue& record)
{
    const size_t dataWidth = params.GetDataWidth();
    switch (fieldId) {
    case OptionalFieldId::Opcode:
        return setOptionalFieldValue(
            record,
            Detail::DecodeOpcode<CHI::B::Opcodes::DAT::Decoder<FlexibleBDatFlit>>(flit.Opcode()),
            Detail::HexValue(static_cast<qulonglong>(flit.Opcode())));
    case OptionalFieldId::SrcID:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.SrcID()));
    case OptionalFieldId::TgtID:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.TgtID()));
    case OptionalFieldId::TxnID:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.TxnID()));
    case OptionalFieldId::HomeNID:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.HomeNID()));
    case OptionalFieldId::RespErr:
        return setOptionalFieldValue(record,
                                     Detail::EnumName(CHI::B::RespErrs::ToEnum(flit.RespErr())),
                                     Detail::RawIntegral(static_cast<qulonglong>(flit.RespErr())));
    case OptionalFieldId::Resp:
        return setOptionalFieldValue(record,
                                     Detail::EnumName(CHI::B::Resps::ToEnum(flit.Resp())),
                                     Detail::RawIntegral(static_cast<qulonglong>(flit.Resp())));
    case OptionalFieldId::FwdState:
        return setOptionalFieldValue(
            record,
            Detail::EnumName(CHI::B::FwdStates::ToEnum(
                CHI::B::FwdState(static_cast<unsigned int>(flit.FwdState())))),
            Detail::RawIntegral(static_cast<qulonglong>(flit.FwdState())));
    case OptionalFieldId::DataSource:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.DataSource()));
    case OptionalFieldId::DBID:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.DBID()));
    case OptionalFieldId::CCID:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.CCID()));
    case OptionalFieldId::DataID:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.DataID()));
    case OptionalFieldId::TraceTag:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.TraceTag()));
    case OptionalFieldId::RSVDC:
        return params.GetDatRSVDCWidth() > 0
            && setFixedIntegralOptionalField(record,
                                             static_cast<qulonglong>(flit.RSVDC()),
                                             params.GetDatRSVDCWidth());
    case OptionalFieldId::BE:
        return setOptionalFieldValue(record,
                                     formatFixedHex(static_cast<qulonglong>(flit.BE()), dataWidth / 8),
                                     formatFixedRawIntegral(static_cast<qulonglong>(flit.BE()), dataWidth / 8));
    case OptionalFieldId::Data:
        return setOptionalFieldValue(record,
                                     formatDataValue(flit, dataWidth),
                                     formatDataRaw(flit, dataWidth));
    case OptionalFieldId::DataCheck:
        return params.IsDataCheckPresent()
            && setOptionalFieldValue(record,
                                     formatFixedHex(static_cast<qulonglong>(flit.DataCheck()), dataWidth / 8),
                                     formatFixedRawIntegral(static_cast<qulonglong>(flit.DataCheck()),
                                                            dataWidth / 8));
    case OptionalFieldId::Poison:
        return params.IsPoisonPresent()
            && setFixedIntegralOptionalField(record,
                                             static_cast<qulonglong>(flit.Poison()),
                                             dataWidth / 64);
    default:
        return false;
    }
}

bool extractBSnpOptionalField(const CLog::Parameters& params,
                              const OptionalFieldId fieldId,
                              const FlexibleBSnpFlit& flit,
                              CLogBTraceOptionalFieldValue& record)
{
    switch (fieldId) {
    case OptionalFieldId::Opcode:
        return setOptionalFieldValue(
            record,
            Detail::DecodeOpcode<CHI::B::Opcodes::SNP::Decoder<FlexibleBSnpFlit>>(flit.Opcode()),
            Detail::HexValue(static_cast<qulonglong>(flit.Opcode())));
    case OptionalFieldId::SrcID:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.SrcID()));
    case OptionalFieldId::TxnID:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.TxnID()));
    case OptionalFieldId::FwdNID:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.FwdNID()));
    case OptionalFieldId::FwdTxnID:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.FwdTxnID()));
    case OptionalFieldId::Addr:
        return setOptionalFieldValue(record,
                                     formatFixedHex(static_cast<qulonglong>(flit.Addr() << 3),
                                                    params.GetReqAddrWidth()),
                                     Detail::RawIntegral(static_cast<qulonglong>(flit.Addr())));
    case OptionalFieldId::NS:
        return setOptionalFieldValue(record,
                                     Detail::EnumName(CHI::B::NSs::ToEnum(flit.NS())),
                                     Detail::RawIntegral(static_cast<qulonglong>(flit.NS())));
    case OptionalFieldId::DoNotGoToSD:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.DoNotGoToSD()));
    case OptionalFieldId::RetToSrc:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.RetToSrc()));
    case OptionalFieldId::TraceTag:
        return setIntegralOptionalField(record, static_cast<qulonglong>(flit.TraceTag()));
    default:
        return false;
    }
}

bool buildEbMeasures(const CLog::Parameters& params,
                     EbMeasures& measures,
                     CLogBTraceLoadError& error)
{
    CHI::Eb::Flits::REQMeasure::Parameters reqParams{
        .nodeIdWidth = params.GetNodeIdWidth(),
        .reqAddrWidth = params.GetReqAddrWidth(),
        .reqRsvdcWidth = params.GetReqRSVDCWidth(),
        .mpamPresent = params.IsMPAMPresent(),
    };
    CHI::Eb::Flits::RSPMeasure::Parameters rspParams{
        .nodeIdWidth = params.GetNodeIdWidth(),
    };
    CHI::Eb::Flits::DATMeasure::Parameters datParams{
        .nodeIdWidth = params.GetNodeIdWidth(),
        .dataWidth = params.GetDataWidth(),
        .datRsvdcWidth = params.GetDatRSVDCWidth(),
        .dataCheckPresent = params.IsDataCheckPresent(),
        .poisonPresent = params.IsPoisonPresent(),
    };
    CHI::Eb::Flits::SNPMeasure::Parameters snpParams{
        .nodeIdWidth = params.GetNodeIdWidth(),
        .reqAddrWidth = params.GetReqAddrWidth(),
        .mpamPresent = params.IsMPAMPresent(),
    };

    if (!measures.req.Eval(reqParams)
        || !measures.rsp.Eval(rspParams)
        || !measures.dat.Eval(datParams)
        || !measures.snp.Eval(snpParams)) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("Unable to evaluate CHI flit measurements for this CLog.B file."),
                     QStringLiteral("The file declares unsupported or inconsistent CHI parameters."),
                     describeParametersBlock(params));
        return false;
    }

    return true;
}

bool buildBMeasures(const CLog::Parameters& params,
                    BMeasures& measures,
                    CLogBTraceLoadError& error)
{
    CHI::B::Flits::REQMeasure::Parameters reqParams{
        .nodeIdWidth = params.GetNodeIdWidth(),
        .reqAddrWidth = params.GetReqAddrWidth(),
        .reqRsvdcWidth = params.GetReqRSVDCWidth(),
    };
    CHI::B::Flits::RSPMeasure::Parameters rspParams{
        .nodeIdWidth = params.GetNodeIdWidth(),
    };
    CHI::B::Flits::DATMeasure::Parameters datParams{
        .nodeIdWidth = params.GetNodeIdWidth(),
        .dataWidth = params.GetDataWidth(),
        .datRsvdcWidth = params.GetDatRSVDCWidth(),
        .dataCheckPresent = params.IsDataCheckPresent(),
        .poisonPresent = params.IsPoisonPresent(),
    };
    CHI::B::Flits::SNPMeasure::Parameters snpParams{
        .nodeIdWidth = params.GetNodeIdWidth(),
        .reqAddrWidth = params.GetReqAddrWidth(),
    };

    if (!measures.req.Eval(reqParams)
        || !measures.rsp.Eval(rspParams)
        || !measures.dat.Eval(datParams)
        || !measures.snp.Eval(snpParams)) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("Unable to evaluate CHI flit measurements for this CLog.B file."),
                     QStringLiteral("The file declares unsupported or inconsistent CHI parameters."),
                     describeParametersBlock(params));
        return false;
    }

    return true;
}

bool deserializeReq(const CHI::Eb::Flits::REQMeasure& measure,
                    FlexibleReqFlit& flit,
                    uint32_t* flitBits,
                    const size_t bitLength)
{
    const size_t expectedBitLength = measure.width._;
    if (bitLength != byteAlignedBitLength(expectedBitLength)
        || expectedBitLength > FlexibleReqFlit::WIDTH) {
        return false;
    }

    flit = FlexibleReqFlit();
    CHI::Eb::Flits::details::FlitWalker walker(flitBits);

    flit.QoS() = walker.Walk32(measure.width.QoS);
    flit.TgtID() = walker.Walk32(measure.width.TgtID);
    flit.SrcID() = walker.Walk32(measure.width.SrcID);
    flit.TxnID() = walker.Walk32(measure.width.TxnID);
    flit.ReturnNID() = walker.Walk32(measure.width.ReturnNID);
    flit.StashNIDValid() = walker.Walk32(measure.width.StashNIDValid);
    flit.ReturnTxnID() = walker.Walk32(measure.width.ReturnTxnID);
    flit.Opcode() = walker.Walk32(measure.width.Opcode);
    flit.Size() = walker.Walk32(measure.width.Size);
    flit.Addr() = uint64_t(walker.Walk32(32))
                | (uint64_t(walker.Walk32(measure.width.Addr - 32)) << 32);
    flit.NS() = walker.Walk32(measure.width.NS);
    flit.LikelyShared() = walker.Walk32(measure.width.LikelyShared);
    flit.AllowRetry() = walker.Walk32(measure.width.AllowRetry);
    flit.Order() = walker.Walk32(measure.width.Order);
    flit.PCrdType() = walker.Walk32(measure.width.PCrdType);
    flit.MemAttr() = walker.Walk32(measure.width.MemAttr);
    flit.SnpAttr() = walker.Walk32(measure.width.SnpAttr);
    flit.TagGroupID() = walker.Walk32(measure.width.TagGroupID);
    flit.Excl() = walker.Walk32(measure.width.Excl);
    flit.ExpCompAck() = walker.Walk32(measure.width.ExpCompAck);
    flit.TagOp() = walker.Walk32(measure.width.TagOp);
    flit.TraceTag() = walker.Walk32(measure.width.TraceTag);
    if (measure.width.MPAM > 0) {
        flit.MPAM() = walker.Walk32(measure.width.MPAM);
    }
    if (measure.width.RSVDC > 0) {
        flit.RSVDC() = walker.Walk32(measure.width.RSVDC);
    }

    walker.Finish(expectedBitLength);
    return true;
}

bool deserializeRsp(const CHI::Eb::Flits::RSPMeasure& measure,
                    FlexibleRspFlit& flit,
                    uint32_t* flitBits,
                    const size_t bitLength)
{
    const size_t expectedBitLength = measure.width._;
    if (bitLength != byteAlignedBitLength(expectedBitLength)
        || expectedBitLength > FlexibleRspFlit::WIDTH) {
        return false;
    }

    flit = FlexibleRspFlit();
    CHI::Eb::Flits::details::FlitWalker walker(flitBits);

    flit.QoS() = walker.Walk32(measure.width.QoS);
    flit.TgtID() = walker.Walk32(measure.width.TgtID);
    flit.SrcID() = walker.Walk32(measure.width.SrcID);
    flit.TxnID() = walker.Walk32(measure.width.TxnID);
    flit.Opcode() = walker.Walk32(measure.width.Opcode);
    flit.RespErr() = walker.Walk32(measure.width.RespErr);
    flit.Resp() = walker.Walk32(measure.width.Resp);
    flit.FwdState() = walker.Walk32(measure.width.FwdState);
    flit.CBusy() = walker.Walk32(measure.width.CBusy);
    flit.DBID() = walker.Walk32(measure.width.DBID);
    flit.PCrdType() = walker.Walk32(measure.width.PCrdType);
    flit.TagOp() = walker.Walk32(measure.width.TagOp);
    flit.TraceTag() = walker.Walk32(measure.width.TraceTag);

    walker.Finish(expectedBitLength);
    return true;
}

bool deserializeDat(const CHI::Eb::Flits::DATMeasure& measure,
                    FlexibleDatFlit& flit,
                    uint32_t* flitBits,
                    const size_t bitLength)
{
    const size_t expectedBitLength = measure.width._;
    if (bitLength != byteAlignedBitLength(expectedBitLength)
        || expectedBitLength > FlexibleDatFlit::WIDTH) {
        return false;
    }

    flit = FlexibleDatFlit();
    CHI::Eb::Flits::details::FlitWalker walker(flitBits);

    flit.QoS() = walker.Walk32(measure.width.QoS);
    flit.TgtID() = walker.Walk32(measure.width.TgtID);
    flit.SrcID() = walker.Walk32(measure.width.SrcID);
    flit.TxnID() = walker.Walk32(measure.width.TxnID);
    flit.HomeNID() = walker.Walk32(measure.width.HomeNID);
    flit.Opcode() = walker.Walk32(measure.width.Opcode);
    flit.RespErr() = walker.Walk32(measure.width.RespErr);
    flit.Resp() = walker.Walk32(measure.width.Resp);
    flit.DataSource() = walker.Walk32(measure.width.DataSource);
    flit.CBusy() = walker.Walk32(measure.width.CBusy);
    flit.DBID() = walker.Walk32(measure.width.DBID);
    flit.CCID() = walker.Walk32(measure.width.CCID);
    flit.DataID() = walker.Walk32(measure.width.DataID);
    flit.TagOp() = walker.Walk32(measure.width.TagOp);
    flit.Tag() = walker.Walk32(measure.width.Tag);
    flit.TU() = walker.Walk32(measure.width.TU);
    flit.TraceTag() = walker.Walk32(measure.width.TraceTag);
    if (measure.width.RSVDC > 0) {
        flit.RSVDC() = walker.Walk32(measure.width.RSVDC);
    }
    if (measure.width.BE > 32) {
        flit.BE() = uint64_t(walker.Walk32(32))
                  | (uint64_t(walker.Walk32(measure.width.BE - 32)) << 32);
    } else {
        flit.BE() = walker.Walk32(measure.width.BE);
    }

    const size_t dataWords = measure.width.Data / 64;
    for (size_t index = 0; index < dataWords; ++index) {
        flit.Data()[index] = uint64_t(walker.Walk32(32))
                           | (uint64_t(walker.Walk32(32)) << 32);
    }

    if (measure.width.DataCheck > 0) {
        if (measure.width.DataCheck > 32) {
            flit.DataCheck() = uint64_t(walker.Walk32(32))
                             | (uint64_t(walker.Walk32(measure.width.DataCheck - 32)) << 32);
        } else {
            flit.DataCheck() = walker.Walk32(measure.width.DataCheck);
        }
    }

    if (measure.width.Poison > 0) {
        flit.Poison() = walker.Walk32(measure.width.Poison);
    }

    walker.Finish(expectedBitLength);
    return true;
}

bool deserializeSnp(const CHI::Eb::Flits::SNPMeasure& measure,
                    FlexibleSnpFlit& flit,
                    uint32_t* flitBits,
                    const size_t bitLength)
{
    const size_t expectedBitLength = measure.width._;
    if (bitLength != byteAlignedBitLength(expectedBitLength)
        || expectedBitLength > FlexibleSnpFlit::WIDTH) {
        return false;
    }

    flit = FlexibleSnpFlit();
    CHI::Eb::Flits::details::FlitWalker walker(flitBits);

    flit.QoS() = walker.Walk32(measure.width.QoS);
    flit.SrcID() = walker.Walk32(measure.width.SrcID);
    flit.TxnID() = walker.Walk32(measure.width.TxnID);
    flit.FwdNID() = walker.Walk32(measure.width.FwdNID);
    flit.FwdTxnID() = walker.Walk32(measure.width.FwdTxnID);
    flit.Opcode() = walker.Walk32(measure.width.Opcode);
    flit.Addr() = uint64_t(walker.Walk32(32))
                | (uint64_t(walker.Walk32(measure.width.Addr - 32)) << 32);
    flit.NS() = walker.Walk32(measure.width.NS);
    flit.DoNotGoToSD() = walker.Walk32(measure.width.DoNotGoToSD);
    flit.RetToSrc() = walker.Walk32(measure.width.RetToSrc);
    flit.TraceTag() = walker.Walk32(measure.width.TraceTag);
    if (measure.width.MPAM > 0) {
        flit.MPAM() = walker.Walk32(measure.width.MPAM);
    }

    walker.Finish(expectedBitLength);
    return true;
}

bool deserializeBReq(const CHI::B::Flits::REQMeasure& measure,
                     FlexibleBReqFlit& flit,
                     uint32_t* flitBits,
                     const size_t bitLength)
{
    const size_t expectedBitLength = measure.width._;
    if (bitLength != byteAlignedBitLength(expectedBitLength)
        || expectedBitLength > FlexibleBReqFlit::WIDTH) {
        return false;
    }

    flit = FlexibleBReqFlit();
    CHI::B::Flits::details::FlitWalker walker(flitBits);

    flit.QoS() = walker.Walk32(measure.width.QoS);
    flit.TgtID() = walker.Walk32(measure.width.TgtID);
    flit.SrcID() = walker.Walk32(measure.width.SrcID);
    flit.TxnID() = walker.Walk32(measure.width.TxnID);
    flit.ReturnNID() = walker.Walk32(measure.width.ReturnNID);
    flit.StashNIDValid() = walker.Walk32(measure.width.StashNIDValid);
    flit.ReturnTxnID() = walker.Walk32(measure.width.ReturnTxnID);
    flit.Opcode() = walker.Walk32(measure.width.Opcode);
    flit.Size() = walker.Walk32(measure.width.Size);
    flit.Addr() = uint64_t(walker.Walk32(32))
                | (uint64_t(walker.Walk32(measure.width.Addr - 32)) << 32);
    flit.NS() = walker.Walk32(measure.width.NS);
    flit.LikelyShared() = walker.Walk32(measure.width.LikelyShared);
    flit.AllowRetry() = walker.Walk32(measure.width.AllowRetry);
    flit.Order() = walker.Walk32(measure.width.Order);
    flit.PCrdType() = walker.Walk32(measure.width.PCrdType);
    flit.MemAttr() = walker.Walk32(measure.width.MemAttr);
    flit.SnpAttr() = walker.Walk32(measure.width.SnpAttr);
    flit.LPID() = walker.Walk32(measure.width.LPID);
    flit.Excl() = walker.Walk32(measure.width.Excl);
    flit.ExpCompAck() = walker.Walk32(measure.width.ExpCompAck);
    flit.TraceTag() = walker.Walk32(measure.width.TraceTag);
    if (measure.width.RSVDC > 0) {
        flit.RSVDC() = walker.Walk32(measure.width.RSVDC);
    }

    walker.Finish(expectedBitLength);
    return true;
}

bool deserializeBRsp(const CHI::B::Flits::RSPMeasure& measure,
                     FlexibleBRspFlit& flit,
                     uint32_t* flitBits,
                     const size_t bitLength)
{
    const size_t expectedBitLength = measure.width._;
    if (bitLength != byteAlignedBitLength(expectedBitLength)
        || expectedBitLength > FlexibleBRspFlit::WIDTH) {
        return false;
    }

    flit = FlexibleBRspFlit();
    CHI::B::Flits::details::FlitWalker walker(flitBits);

    flit.QoS() = walker.Walk32(measure.width.QoS);
    flit.TgtID() = walker.Walk32(measure.width.TgtID);
    flit.SrcID() = walker.Walk32(measure.width.SrcID);
    flit.TxnID() = walker.Walk32(measure.width.TxnID);
    flit.Opcode() = walker.Walk32(measure.width.Opcode);
    flit.RespErr() = walker.Walk32(measure.width.RespErr);
    flit.Resp() = walker.Walk32(measure.width.Resp);
    flit.FwdState() = walker.Walk32(measure.width.FwdState);
    flit.DBID() = walker.Walk32(measure.width.DBID);
    flit.PCrdType() = walker.Walk32(measure.width.PCrdType);
    flit.TraceTag() = walker.Walk32(measure.width.TraceTag);

    walker.Finish(expectedBitLength);
    return true;
}

bool deserializeBDat(const CHI::B::Flits::DATMeasure& measure,
                     FlexibleBDatFlit& flit,
                     uint32_t* flitBits,
                     const size_t bitLength)
{
    const size_t expectedBitLength = measure.width._;
    if (bitLength != byteAlignedBitLength(expectedBitLength)
        || expectedBitLength > FlexibleBDatFlit::WIDTH) {
        return false;
    }

    flit = FlexibleBDatFlit();
    CHI::B::Flits::details::FlitWalker walker(flitBits);

    flit.QoS() = walker.Walk32(measure.width.QoS);
    flit.TgtID() = walker.Walk32(measure.width.TgtID);
    flit.SrcID() = walker.Walk32(measure.width.SrcID);
    flit.TxnID() = walker.Walk32(measure.width.TxnID);
    flit.HomeNID() = walker.Walk32(measure.width.HomeNID);
    flit.Opcode() = walker.Walk32(measure.width.Opcode);
    flit.RespErr() = walker.Walk32(measure.width.RespErr);
    flit.Resp() = walker.Walk32(measure.width.Resp);
    flit.DataSource() = walker.Walk32(measure.width.DataSource);
    flit.DBID() = walker.Walk32(measure.width.DBID);
    flit.CCID() = walker.Walk32(measure.width.CCID);
    flit.DataID() = walker.Walk32(measure.width.DataID);
    flit.TraceTag() = walker.Walk32(measure.width.TraceTag);
    if (measure.width.RSVDC > 0) {
        flit.RSVDC() = walker.Walk32(measure.width.RSVDC);
    }
    if (measure.width.BE > 32) {
        flit.BE() = uint64_t(walker.Walk32(32))
                  | (uint64_t(walker.Walk32(measure.width.BE - 32)) << 32);
    } else {
        flit.BE() = walker.Walk32(measure.width.BE);
    }

    const size_t dataWords = measure.width.Data / 64;
    for (size_t index = 0; index < dataWords; ++index) {
        flit.Data()[index] = uint64_t(walker.Walk32(32))
                           | (uint64_t(walker.Walk32(32)) << 32);
    }

    if (measure.width.DataCheck > 0) {
        if (measure.width.DataCheck > 32) {
            flit.DataCheck() = uint64_t(walker.Walk32(32))
                             | (uint64_t(walker.Walk32(measure.width.DataCheck - 32)) << 32);
        } else {
            flit.DataCheck() = walker.Walk32(measure.width.DataCheck);
        }
    }

    if (measure.width.Poison > 0) {
        flit.Poison() = walker.Walk32(measure.width.Poison);
    }

    walker.Finish(expectedBitLength);
    return true;
}

bool deserializeBSnp(const CHI::B::Flits::SNPMeasure& measure,
                     FlexibleBSnpFlit& flit,
                     uint32_t* flitBits,
                     const size_t bitLength)
{
    const size_t expectedBitLength = measure.width._;
    if (bitLength != byteAlignedBitLength(expectedBitLength)
        || expectedBitLength > FlexibleBSnpFlit::WIDTH) {
        return false;
    }

    flit = FlexibleBSnpFlit();
    CHI::B::Flits::details::FlitWalker walker(flitBits);

    flit.QoS() = walker.Walk32(measure.width.QoS);
    flit.SrcID() = walker.Walk32(measure.width.SrcID);
    flit.TxnID() = walker.Walk32(measure.width.TxnID);
    flit.FwdNID() = walker.Walk32(measure.width.FwdNID);
    flit.FwdTxnID() = walker.Walk32(measure.width.FwdTxnID);
    flit.Opcode() = walker.Walk32(measure.width.Opcode);
    flit.Addr() = uint64_t(walker.Walk32(32))
                | (uint64_t(walker.Walk32(measure.width.Addr - 32)) << 32);
    flit.NS() = walker.Walk32(measure.width.NS);
    flit.DoNotGoToSD() = walker.Walk32(measure.width.DoNotGoToSD);
    flit.RetToSrc() = walker.Walk32(measure.width.RetToSrc);
    flit.TraceTag() = walker.Walk32(measure.width.TraceTag);

    walker.Finish(expectedBitLength);
    return true;
}

bool appendDecodedTag(const CLog::Parameters& params,
                      const EbMeasures& measures,
                      const CLog::CLogB::TagCHIRecords& tag,
                      const std::unordered_map<quint16, QString>& nodeAnnotations,
                      const std::unordered_map<quint16, CLog::NodeType>& nodeTypes,
                      std::vector<FlitRecord>& rows,
                      CLogBTraceLoadError& error,
                      const CLogBTraceLoadCallbacks& callbacks,
                      const std::uint64_t progressStartBytes,
                      const std::uint64_t progressEndBytes,
                      const std::uint64_t totalBytes,
                      std::stop_token stopToken = {},
                      const bool allowParallelDecode = true,
                      const bool trackXactions = true)
{
    const QString flitReport = buildFlitReport(params, measures);
    const std::size_t rowBase = rows.size();
    rows.resize(rowBase + tag.records.size());
    std::vector<DecodedEbRecord> decodedRecords(tag.records.size());
    std::vector<qulonglong> recordTimestamps;
    std::size_t failedRecordIndex = 0;
    if (!buildRecordTimestamps(tag, recordTimestamps, failedRecordIndex)) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("Record %1 timestamp exceeds the GUI timestamp range.")
                         .arg(static_cast<qulonglong>(failedRecordIndex + 1)));
        rows.resize(rowBase);
        return false;
    }
    const std::size_t workerCount = suggestedDecodeWorkerCount(tag.records.size(), allowParallelDecode);
    const std::size_t recordsPerWorker = workerCount == 0
        ? 0
        : (tag.records.size() + workerCount - 1) / workerCount;
    std::vector<std::size_t> recordBytePrefix(tag.records.size() + 1, 0);
    for (std::size_t index = 0; index < tag.records.size(); ++index) {
        recordBytePrefix[index + 1] = recordBytePrefix[index] + recordStorageBytes(tag.records[index]);
    }
    std::vector<std::atomic<std::size_t>> workerReportedBytes(workerCount);
    for (std::size_t index = 0; index < workerReportedBytes.size(); ++index) {
        workerReportedBytes[index].store(0, std::memory_order_relaxed);
    }
    std::atomic<std::size_t> completedTagBytes = 0;
    DecodeProgressCallback decodeProgressCallback;
    if (callbacks.stageProgress) {
        decodeProgressCallback =
            [&](const std::size_t completedRecords, const std::size_t totalRecords) {
                callbacks.stageProgress(completedRecords, totalRecords);
            };
    }

    const CLogBTraceLoadWorkerProgressCallback decodeWorkerProgressCallback =
        [&](const CLogBTraceLoadWorkerProgress& workerProgress) {
            if (callbacks.workerProgress) {
                callbacks.workerProgress(workerProgress);
            }

            if (workerProgress.workerIndex >= workerCount) {
                return;
            }

            const std::size_t begin = workerProgress.workerIndex * recordsPerWorker;
            const std::size_t end = std::min(tag.records.size(), begin + recordsPerWorker);
            const std::size_t clampedCompletedRecords = std::min<std::size_t>(
                workerProgress.completedRecords,
                end >= begin ? (end - begin) : 0);
            const std::size_t completedWorkerBytes =
                recordBytePrefix[begin + clampedCompletedRecords] - recordBytePrefix[begin];
            const std::size_t previousWorkerBytes =
                workerReportedBytes[workerProgress.workerIndex].exchange(completedWorkerBytes, std::memory_order_relaxed);

            if (callbacks.progress && completedWorkerBytes >= previousWorkerBytes) {
                const std::size_t totalCompletedBytes = completedTagBytes.fetch_add(
                    completedWorkerBytes - previousWorkerBytes,
                    std::memory_order_relaxed)
                    + (completedWorkerBytes - previousWorkerBytes);
                reportTagDecodeProgress(callbacks.progress,
                                        progressStartBytes,
                                        progressEndBytes,
                                        totalBytes,
                                        totalCompletedBytes,
                                        tag.head.length);
            }
        };

    reportStage(callbacks.stage,
                CLogBTraceLoadStage::Decoding,
                workerCount,
                tag.records.size());

    const auto decodeRecord = [&](const std::size_t recordIndex, CLogBTraceLoadError& workerError) -> bool {
        const auto& source = tag.records[recordIndex];
        const qulonglong timestamp = recordTimestamps[recordIndex];
        const qint64 decodedTimestamp = static_cast<qint64>(timestamp);
        const size_t bitLength = static_cast<size_t>(source.flitLength) * 8U;
        DecodedEbRecord decodedRecord;
        decodedRecord.timestamp = decodedTimestamp;
        decodedRecord.direction = isTxChannel(source.channel) ? FlitDirection::Tx
                                                              : FlitDirection::Rx;
        decodedRecord.nodeId = source.nodeId;
        decodedRecord.sourceChannel = source.channel;

        switch (source.channel) {
        case CLog::Channel::TXREQ:
        case CLog::Channel::TXREQ_BeforeSAM:
        case CLog::Channel::RXREQ:
        case CLog::Channel::RXREQ_BeforeSAM: {
            FlexibleReqFlit flit;
            if (!deserializeReq(measures.req, flit, source.flit.get(), bitLength)) {
                setDeserializationLoadError(workerError,
                                            params,
                                            QStringLiteral("E.b"),
                                            QStringLiteral("REQ"),
                                            tag,
                                            source,
                                            recordIndex,
                                            timestamp,
                                            measures.req,
                                            FlexibleReqFlit::WIDTH);
                workerError.flitReportText = flitReport;
                return false;
            }

            decodedRecord.flit = std::move(flit);
            break;
        }
        case CLog::Channel::TXRSP:
        case CLog::Channel::RXRSP: {
            FlexibleRspFlit flit;
            if (!deserializeRsp(measures.rsp, flit, source.flit.get(), bitLength)) {
                setDeserializationLoadError(workerError,
                                            params,
                                            QStringLiteral("E.b"),
                                            QStringLiteral("RSP"),
                                            tag,
                                            source,
                                            recordIndex,
                                            timestamp,
                                            measures.rsp,
                                            FlexibleRspFlit::WIDTH);
                workerError.flitReportText = flitReport;
                return false;
            }

            decodedRecord.flit = std::move(flit);
            break;
        }
        case CLog::Channel::TXDAT:
        case CLog::Channel::RXDAT: {
            FlexibleDatFlit flit;
            if (!deserializeDat(measures.dat, flit, source.flit.get(), bitLength)) {
                setDeserializationLoadError(workerError,
                                            params,
                                            QStringLiteral("E.b"),
                                            QStringLiteral("DAT"),
                                            tag,
                                            source,
                                            recordIndex,
                                            timestamp,
                                            measures.dat,
                                            FlexibleDatFlit::WIDTH);
                workerError.flitReportText = flitReport;
                return false;
            }

            decodedRecord.flit = std::move(flit);
            break;
        }
        case CLog::Channel::TXSNP:
        case CLog::Channel::RXSNP: {
            FlexibleSnpFlit flit;
            if (!deserializeSnp(measures.snp, flit, source.flit.get(), bitLength)) {
                setDeserializationLoadError(workerError,
                                            params,
                                            QStringLiteral("E.b"),
                                            QStringLiteral("SNP"),
                                            tag,
                                            source,
                                            recordIndex,
                                            timestamp,
                                            measures.snp,
                                            FlexibleSnpFlit::WIDTH);
                workerError.flitReportText = flitReport;
                return false;
            }

            decodedRecord.flit = std::move(flit);
            break;
        }
        }

        decodedRecords[recordIndex] = std::move(decodedRecord);
        return true;
    };

    if (!decodeRecords(tag.records.size(),
                       decodeRecord,
                       error,
                       decodeProgressCallback,
                       decodeWorkerProgressCallback,
                       stopToken,
                       allowParallelDecode)) {
        rows.resize(rowBase);
        return false;
    }

    reportStage(callbacks.stage,
                CLogBTraceLoadStage::Formatting,
                0,
                decodedRecords.size());

    constexpr std::size_t kFormattingProgressBatchSize = 2048;
    auto formatDecodedRecords = [&]<typename XactionConfig>() -> bool {
        EbFlitAdapter adapter;
        CHI::Eb::Xact::Global<XactionConfig> xactGlobal;
        populateEbXactTopology(xactGlobal.TOPOLOGY, nodeTypes);
        CHI::Eb::Xact::RNFJoint<XactionConfig> rnJoint;
        CHI::Eb::Xact::SNFJoint<XactionConfig> snJoint;
        std::uint64_t nextXactionOrdinal = 1;

        for (std::size_t recordIndex = 0; recordIndex < decodedRecords.size(); ++recordIndex) {
            if (stopToken.stop_requested()) {
                setCancelledLoadError(error);
                rows.resize(rowBase);
                return false;
            }

            const DecodedEbRecord& decodedRecord = decodedRecords[recordIndex];
            FlitRecord row;
            QString xactionDebugLog;

            std::visit([&](const auto& flit) {
                using T = std::decay_t<decltype(flit)>;
                if constexpr (std::is_same_v<T, FlexibleReqFlit>) {
                    row = adapter.fromREQ(decodedRecord.timestamp, decodedRecord.direction, flit);
                    if (isBeforeSamChannel(tag.records[recordIndex].channel)) {
                        applyBeforeSamMarker(row);
                    }
                    if (trackXactions) {
                        const auto xactionFlit = makeEbXactionReqFlit<XactionConfig>(flit);
                        const EbXactionProcessResult<XactionConfig> result =
                            processEbReqXaction(xactGlobal,
                                                xactGlobal.TOPOLOGY,
                                                rnJoint,
                                                snJoint,
                                                decodedRecord,
                                                xactionFlit,
                                                nextXactionOrdinal);
                        if (result.ordinal) {
                            appendEbXactionGroupKey(row, *result.ordinal);
                        }
                        xactionDebugLog = buildEbXactionDebugLog(xactGlobal, decodedRecord, row, result);
                    }
                    normalizeAddressDisplay(row,
                                            QStringLiteral("Addr"),
                                            formatFixedHex(static_cast<qulonglong>(flit.Addr()), params.GetReqAddrWidth()));
                    if (params.GetReqRSVDCWidth() > 0) {
                        if (FieldEntry* reqRsvdcField = findField(row.fields, QStringLiteral("RSVDC"))) {
                            reqRsvdcField->raw = formatFixedRawIntegral(static_cast<qulonglong>(flit.RSVDC()),
                                                                        params.GetReqRSVDCWidth());
                        }
                    }
                    normalizeDynamicFields(row, nullptr, params);
                } else if constexpr (std::is_same_v<T, FlexibleRspFlit>) {
                    row = adapter.fromRSP(decodedRecord.timestamp, decodedRecord.direction, flit);
                    if (trackXactions) {
                        const auto xactionFlit = makeEbXactionRspFlit<XactionConfig>(flit);
                        const EbXactionProcessResult<XactionConfig> result =
                            processEbRspXaction(xactGlobal,
                                                xactGlobal.TOPOLOGY,
                                                rnJoint,
                                                snJoint,
                                                decodedRecord,
                                                xactionFlit,
                                                nextXactionOrdinal);
                        if (result.ordinal) {
                            appendEbXactionGroupKey(row, *result.ordinal);
                        }
                        xactionDebugLog = buildEbXactionDebugLog(xactGlobal, decodedRecord, row, result);
                    }
                    normalizeDynamicFields(row, nullptr, params);
                } else if constexpr (std::is_same_v<T, FlexibleDatFlit>) {
                    row = adapter.fromDAT(decodedRecord.timestamp, decodedRecord.direction, flit);
                    if (trackXactions) {
                        const auto xactionFlit = makeEbXactionDatFlit<XactionConfig>(flit);
                        const EbXactionProcessResult<XactionConfig> result =
                            processEbDatXaction(xactGlobal,
                                                xactGlobal.TOPOLOGY,
                                                rnJoint,
                                                snJoint,
                                                decodedRecord,
                                                xactionFlit,
                                                nextXactionOrdinal);
                        if (result.ordinal) {
                            appendEbXactionGroupKey(row, *result.ordinal);
                        }
                        xactionDebugLog = buildEbXactionDebugLog(xactGlobal, decodedRecord, row, result);
                    }
                    normalizeDynamicFields(row, &flit, params);
                } else if constexpr (std::is_same_v<T, FlexibleSnpFlit>) {
                    row = adapter.fromSNP(decodedRecord.timestamp, decodedRecord.direction, flit);
                    if (trackXactions) {
                        const auto xactionFlit = makeEbXactionSnpFlit<XactionConfig>(flit);
                        const EbXactionProcessResult<XactionConfig> result =
                            processEbSnpXaction(xactGlobal,
                                                xactGlobal.TOPOLOGY,
                                                rnJoint,
                                                decodedRecord,
                                                xactionFlit,
                                                nextXactionOrdinal);
                        if (result.ordinal) {
                            appendEbXactionGroupKey(row, *result.ordinal);
                        }
                        xactionDebugLog = buildEbXactionDebugLog(xactGlobal, decodedRecord, row, result);
                    }
                    normalizeAddressDisplay(row,
                                            QStringLiteral("Addr"),
                                            formatFixedHex(static_cast<qulonglong>(flit.Addr() << 3),
                                                           params.GetReqAddrWidth()));
                    normalizeDynamicFields(row, nullptr, params);
                }
            }, decodedRecord.flit);

            row.annotation = captureAnnotationForNode(decodedRecord.nodeId, nodeAnnotations);
            row.channelNodeId = decodedRecord.nodeId;
            attachRawRecordPayload(row, tag.records[recordIndex]);
            AppendFallbackTransactionGroupKeys(row);
            if (trackXactions) {
                appendTransactionKeysToXactionDebugLog(xactionDebugLog, row);
                row.xactionDebugLog = std::move(xactionDebugLog);
            }
            rows[rowBase + recordIndex] = std::move(row);
            if (callbacks.stageProgress
                && (((recordIndex + 1) % kFormattingProgressBatchSize) == 0
                    || (recordIndex + 1) == decodedRecords.size())) {
                callbacks.stageProgress(recordIndex + 1, decodedRecords.size());
            }
        }

        return true;
    };

    switch (params.GetDataWidth()) {
    case 128:
        if (!formatDecodedRecords.template operator()<EbXactionConfig<128>>()) {
            return false;
        }
        break;
    case 256:
        if (!formatDecodedRecords.template operator()<EbXactionConfig<256>>()) {
            return false;
        }
        break;
    case 512:
    default:
        if (!formatDecodedRecords.template operator()<EbXactionConfig<512>>()) {
            return false;
        }
        break;
    }

    reportTagDecodeProgress(callbacks.progress,
                            progressStartBytes,
                            progressEndBytes,
                            totalBytes,
                            tag.records.size(),
                            tag.records.size());

    return true;
}

bool appendDecodedBTag(const CLog::Parameters& params,
                       const BMeasures& measures,
                       const CLog::CLogB::TagCHIRecords& tag,
                       const std::unordered_map<quint16, QString>& nodeAnnotations,
                       std::vector<FlitRecord>& rows,
                       CLogBTraceLoadError& error,
                       const CLogBTraceLoadCallbacks& callbacks,
                       const std::uint64_t progressStartBytes,
                       const std::uint64_t progressEndBytes,
                       const std::uint64_t totalBytes,
                       std::stop_token stopToken = {},
                       const bool allowParallelDecode = true)
{
    const QString flitReport = buildFlitReport(params, measures);
    const std::size_t rowBase = rows.size();
    rows.resize(rowBase + tag.records.size());
    std::vector<DecodedBRecord> decodedRecords(tag.records.size());
    std::vector<qulonglong> recordTimestamps;
    std::size_t failedRecordIndex = 0;
    if (!buildRecordTimestamps(tag, recordTimestamps, failedRecordIndex)) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("Record %1 timestamp exceeds the GUI timestamp range.")
                         .arg(static_cast<qulonglong>(failedRecordIndex + 1)));
        rows.resize(rowBase);
        return false;
    }
    const std::size_t workerCount = suggestedDecodeWorkerCount(tag.records.size(), allowParallelDecode);
    const std::size_t recordsPerWorker = workerCount == 0
        ? 0
        : (tag.records.size() + workerCount - 1) / workerCount;
    std::vector<std::size_t> recordBytePrefix(tag.records.size() + 1, 0);
    for (std::size_t index = 0; index < tag.records.size(); ++index) {
        recordBytePrefix[index + 1] = recordBytePrefix[index] + recordStorageBytes(tag.records[index]);
    }
    std::vector<std::atomic<std::size_t>> workerReportedBytes(workerCount);
    for (std::size_t index = 0; index < workerReportedBytes.size(); ++index) {
        workerReportedBytes[index].store(0, std::memory_order_relaxed);
    }
    std::atomic<std::size_t> completedTagBytes = 0;
    DecodeProgressCallback decodeProgressCallback;
    if (callbacks.stageProgress) {
        decodeProgressCallback =
            [&](const std::size_t completedRecords, const std::size_t totalRecords) {
                callbacks.stageProgress(completedRecords, totalRecords);
            };
    }

    const CLogBTraceLoadWorkerProgressCallback decodeWorkerProgressCallback =
        [&](const CLogBTraceLoadWorkerProgress& workerProgress) {
            if (callbacks.workerProgress) {
                callbacks.workerProgress(workerProgress);
            }

            if (workerProgress.workerIndex >= workerCount) {
                return;
            }

            const std::size_t begin = workerProgress.workerIndex * recordsPerWorker;
            const std::size_t end = std::min(tag.records.size(), begin + recordsPerWorker);
            const std::size_t clampedCompletedRecords = std::min<std::size_t>(
                workerProgress.completedRecords,
                end >= begin ? (end - begin) : 0);
            const std::size_t completedWorkerBytes =
                recordBytePrefix[begin + clampedCompletedRecords] - recordBytePrefix[begin];
            const std::size_t previousWorkerBytes =
                workerReportedBytes[workerProgress.workerIndex].exchange(completedWorkerBytes, std::memory_order_relaxed);

            if (callbacks.progress && completedWorkerBytes >= previousWorkerBytes) {
                const std::size_t totalCompletedBytes = completedTagBytes.fetch_add(
                    completedWorkerBytes - previousWorkerBytes,
                    std::memory_order_relaxed)
                    + (completedWorkerBytes - previousWorkerBytes);
                reportTagDecodeProgress(callbacks.progress,
                                        progressStartBytes,
                                        progressEndBytes,
                                        totalBytes,
                                        totalCompletedBytes,
                                        tag.head.length);
            }
        };

    reportStage(callbacks.stage,
                CLogBTraceLoadStage::Decoding,
                workerCount,
                tag.records.size());

    const auto decodeRecord = [&](const std::size_t recordIndex, CLogBTraceLoadError& workerError) -> bool {
        const auto& source = tag.records[recordIndex];
        const qulonglong timestamp = recordTimestamps[recordIndex];
        const qint64 decodedTimestamp = static_cast<qint64>(timestamp);
        const size_t bitLength = static_cast<size_t>(source.flitLength) * 8U;
        DecodedBRecord decodedRecord;
        decodedRecord.timestamp = decodedTimestamp;
        decodedRecord.direction = isTxChannel(source.channel) ? FlitDirection::Tx
                                                              : FlitDirection::Rx;
        decodedRecord.nodeId = source.nodeId;

        switch (source.channel) {
        case CLog::Channel::TXREQ:
        case CLog::Channel::TXREQ_BeforeSAM:
        case CLog::Channel::RXREQ_BeforeSAM:
        case CLog::Channel::RXREQ: {
            FlexibleBReqFlit flit;
            if (!deserializeBReq(measures.req, flit, source.flit.get(), bitLength)) {
                setDeserializationLoadError(workerError,
                                            params,
                                            QStringLiteral("B"),
                                            QStringLiteral("REQ"),
                                            tag,
                                            source,
                                            recordIndex,
                                            timestamp,
                                            measures.req,
                                            FlexibleBReqFlit::WIDTH);
                workerError.flitReportText = flitReport;
                return false;
            }

            decodedRecord.flit = std::move(flit);
            break;
        }
        case CLog::Channel::TXRSP:
        case CLog::Channel::RXRSP: {
            FlexibleBRspFlit flit;
            if (!deserializeBRsp(measures.rsp, flit, source.flit.get(), bitLength)) {
                setDeserializationLoadError(workerError,
                                            params,
                                            QStringLiteral("B"),
                                            QStringLiteral("RSP"),
                                            tag,
                                            source,
                                            recordIndex,
                                            timestamp,
                                            measures.rsp,
                                            FlexibleBRspFlit::WIDTH);
                workerError.flitReportText = flitReport;
                return false;
            }

            decodedRecord.flit = std::move(flit);
            break;
        }
        case CLog::Channel::TXDAT:
        case CLog::Channel::RXDAT: {
            FlexibleBDatFlit flit;
            if (!deserializeBDat(measures.dat, flit, source.flit.get(), bitLength)) {
                setDeserializationLoadError(workerError,
                                            params,
                                            QStringLiteral("B"),
                                            QStringLiteral("DAT"),
                                            tag,
                                            source,
                                            recordIndex,
                                            timestamp,
                                            measures.dat,
                                            FlexibleBDatFlit::WIDTH);
                workerError.flitReportText = flitReport;
                return false;
            }

            decodedRecord.flit = std::move(flit);
            break;
        }
        case CLog::Channel::TXSNP:
        case CLog::Channel::RXSNP: {
            FlexibleBSnpFlit flit;
            if (!deserializeBSnp(measures.snp, flit, source.flit.get(), bitLength)) {
                setDeserializationLoadError(workerError,
                                            params,
                                            QStringLiteral("B"),
                                            QStringLiteral("SNP"),
                                            tag,
                                            source,
                                            recordIndex,
                                            timestamp,
                                            measures.snp,
                                            FlexibleBSnpFlit::WIDTH);
                workerError.flitReportText = flitReport;
                return false;
            }

            decodedRecord.flit = std::move(flit);
            break;
        }
        }

        decodedRecords[recordIndex] = std::move(decodedRecord);
        return true;
    };

    if (!decodeRecords(tag.records.size(),
                       decodeRecord,
                       error,
                       decodeProgressCallback,
                       decodeWorkerProgressCallback,
                       stopToken,
                       allowParallelDecode)) {
        rows.resize(rowBase);
        return false;
    }

    reportStage(callbacks.stage,
                CLogBTraceLoadStage::Formatting,
                0,
                decodedRecords.size());

    BFlitAdapter adapter;
    constexpr std::size_t kFormattingProgressBatchSize = 2048;
    for (std::size_t recordIndex = 0; recordIndex < decodedRecords.size(); ++recordIndex) {
        if (stopToken.stop_requested()) {
            setCancelledLoadError(error);
            rows.resize(rowBase);
            return false;
        }

        const DecodedBRecord& decodedRecord = decodedRecords[recordIndex];
        FlitRecord row;

        std::visit([&](const auto& flit) {
            using T = std::decay_t<decltype(flit)>;
                if constexpr (std::is_same_v<T, FlexibleBReqFlit>) {
                    row = adapter.fromREQ(decodedRecord.timestamp, decodedRecord.direction, flit);
                    if (isBeforeSamChannel(tag.records[recordIndex].channel)) {
                        applyBeforeSamMarker(row);
                    }
                    normalizeAddressDisplay(row,
                                        QStringLiteral("Addr"),
                                        formatFixedHex(static_cast<qulonglong>(flit.Addr()), params.GetReqAddrWidth()));
                if (params.GetReqRSVDCWidth() > 0) {
                    if (FieldEntry* reqRsvdcField = findField(row.fields, QStringLiteral("RSVDC"))) {
                        reqRsvdcField->raw = formatFixedRawIntegral(static_cast<qulonglong>(flit.RSVDC()),
                                                                    params.GetReqRSVDCWidth());
                    }
                }
                normalizeDynamicFields(row, nullptr, params);
            } else if constexpr (std::is_same_v<T, FlexibleBRspFlit>) {
                row = adapter.fromRSP(decodedRecord.timestamp, decodedRecord.direction, flit);
                normalizeDynamicFields(row, nullptr, params);
            } else if constexpr (std::is_same_v<T, FlexibleBDatFlit>) {
                row = adapter.fromDAT(decodedRecord.timestamp, decodedRecord.direction, flit);
                normalizeDynamicFields(row, &flit, params);
            } else if constexpr (std::is_same_v<T, FlexibleBSnpFlit>) {
                row = adapter.fromSNP(decodedRecord.timestamp, decodedRecord.direction, flit);
                normalizeAddressDisplay(row,
                                        QStringLiteral("Addr"),
                                        formatFixedHex(static_cast<qulonglong>(flit.Addr() << 3),
                                                       params.GetReqAddrWidth()));
                normalizeDynamicFields(row, nullptr, params);
            }
        }, decodedRecord.flit);

        row.annotation = captureAnnotationForNode(decodedRecord.nodeId, nodeAnnotations);
        row.channelNodeId = decodedRecord.nodeId;
        attachRawRecordPayload(row, tag.records[recordIndex]);
        AppendFallbackTransactionGroupKeys(row);
        rows[rowBase + recordIndex] = std::move(row);
        if (callbacks.stageProgress
            && (((recordIndex + 1) % kFormattingProgressBatchSize) == 0
                || (recordIndex + 1) == decodedRecords.size())) {
            callbacks.stageProgress(recordIndex + 1, decodedRecords.size());
        }
    }

    reportTagDecodeProgress(callbacks.progress,
                            progressStartBytes,
                            progressEndBytes,
                            totalBytes,
                            tag.records.size(),
                            tag.records.size());

    return true;
}

bool prepareEbTraceDecoding(const CLog::Parameters& params,
                            EbMeasures& measures,
                            CLogBTraceLoadError& error)
{
    return buildEbMeasures(params, measures, error);
}

bool prepareBTraceDecoding(const CLog::Parameters& params,
                           BMeasures& measures,
                           CLogBTraceLoadError& error)
{
    return buildBMeasures(params, measures, error);
}

std::uint8_t storedByteLengthForWidth(const std::size_t bitWidth) noexcept
{
    return static_cast<std::uint8_t>(byteAlignedBitLength(bitWidth) / 8U);
}

std::array<std::uint8_t, 4> expectedByteLengthsFor(const EbMeasures& measures) noexcept
{
    return {
        storedByteLengthForWidth(measures.req.width._),
        storedByteLengthForWidth(measures.rsp.width._),
        storedByteLengthForWidth(measures.dat.width._),
        storedByteLengthForWidth(measures.snp.width._),
    };
}

std::array<std::uint8_t, 4> expectedByteLengthsFor(const BMeasures& measures) noexcept
{
    return {
        storedByteLengthForWidth(measures.req.width._),
        storedByteLengthForWidth(measures.rsp.width._),
        storedByteLengthForWidth(measures.dat.width._),
        storedByteLengthForWidth(measures.snp.width._),
    };
}

std::array<std::uint16_t, 4> expectedBitWidthsFor(const EbMeasures& measures) noexcept
{
    return {
        static_cast<std::uint16_t>(measures.req.width._),
        static_cast<std::uint16_t>(measures.rsp.width._),
        static_cast<std::uint16_t>(measures.dat.width._),
        static_cast<std::uint16_t>(measures.snp.width._),
    };
}

std::array<std::uint16_t, 4> expectedBitWidthsFor(const BMeasures& measures) noexcept
{
    return {
        static_cast<std::uint16_t>(measures.req.width._),
        static_cast<std::uint16_t>(measures.rsp.width._),
        static_cast<std::uint16_t>(measures.dat.width._),
        static_cast<std::uint16_t>(measures.snp.width._),
    };
}

bool appendMatchingGuess(const CLogBTraceFlitLengthMasks& observedMasks,
                         const CLog::Parameters& params,
                         const std::array<std::uint16_t, 4>& expectedBitWidths,
                         const std::array<std::uint8_t, 4>& expectedByteLengths,
                         std::vector<CLogBTraceConfigurationGuess>& guesses)
{
    std::array<bool, 4> matchedChannels{};
    for (std::size_t channelIndex = 0; channelIndex < observedMasks.size(); ++channelIndex) {
        if (flitLengthMaskIsEmpty(observedMasks[channelIndex])) {
            continue;
        }

        matchedChannels[channelIndex] = true;
        if (!flitLengthMaskMatchesOnly(observedMasks[channelIndex],
                                       expectedByteLengths[channelIndex])) {
            return false;
        }
    }

    guesses.push_back(CLogBTraceConfigurationGuess{
        .parameters = params,
        .expectedBitWidths = expectedBitWidths,
        .expectedByteLengths = expectedByteLengths,
        .matchedChannels = matchedChannels,
    });
    return true;
}

int issueSortValue(const CLog::Issue issue) noexcept
{
    switch (issue) {
    case CLog::Issue::B:
        return 0;
    case CLog::Issue::Eb:
        return 1;
    }

    return 2;
}

int parameterDifferenceScore(const CLog::Parameters& lhs,
                             const CLog::Parameters& rhs) noexcept
{
    int score = 0;
    score += lhs.GetIssue() != rhs.GetIssue() ? 1 : 0;
    score += lhs.GetNodeIdWidth() != rhs.GetNodeIdWidth() ? 1 : 0;
    score += lhs.GetReqAddrWidth() != rhs.GetReqAddrWidth() ? 1 : 0;
    score += lhs.GetReqRSVDCWidth() != rhs.GetReqRSVDCWidth() ? 1 : 0;
    score += lhs.GetDatRSVDCWidth() != rhs.GetDatRSVDCWidth() ? 1 : 0;
    score += lhs.GetDataWidth() != rhs.GetDataWidth() ? 1 : 0;
    score += lhs.IsDataCheckPresent() != rhs.IsDataCheckPresent() ? 1 : 0;
    score += lhs.IsPoisonPresent() != rhs.IsPoisonPresent() ? 1 : 0;
    score += lhs.IsMPAMPresent() != rhs.IsMPAMPresent() ? 1 : 0;
    return score;
}

bool parameterSortLess(const CLogBTraceConfigurationGuess& lhs,
                       const CLogBTraceConfigurationGuess& rhs,
                       const CLog::Parameters& declaredParameters) noexcept
{
    const int lhsScore = parameterDifferenceScore(lhs.parameters, declaredParameters);
    const int rhsScore = parameterDifferenceScore(rhs.parameters, declaredParameters);
    if (lhsScore != rhsScore) {
        return lhsScore < rhsScore;
    }

    const auto key = [](const CLog::Parameters& params) {
        return std::tuple(
            issueSortValue(params.GetIssue()),
            params.GetNodeIdWidth(),
            params.GetReqAddrWidth(),
            params.GetReqRSVDCWidth(),
            params.GetDatRSVDCWidth(),
            params.GetDataWidth(),
            params.IsDataCheckPresent(),
            params.IsPoisonPresent(),
            params.IsMPAMPresent());
    };

    return key(lhs.parameters) < key(rhs.parameters);
}

template<typename DecoderT>
void appendSupportedOpcodeNames(QStringList& names, QSet<QString>& seen)
{
    const DecoderT& decoder = DecoderT::INSTANCE;
    auto iterator = decoder.Iterate();
    while (true) {
        const auto& opcodeInfo = iterator.Next();
        if (!opcodeInfo.IsValid()) {
            break;
        }

        const QString name = QString::fromLatin1(opcodeInfo.GetName("Unknown"));
        if (!seen.contains(name)) {
            seen.insert(name);
            names.append(name);
        }
    }
}

template<typename DecoderT, typename... ChannelValuesT>
void appendOpcodeMatchesForChannels(const QString& filter,
                                    ChannelValuesT&... channelValues)
{
    const DecoderT& decoder = DecoderT::INSTANCE;
    auto iterator = decoder.Iterate();
    while (true) {
        const auto& opcodeInfo = iterator.Next();
        if (!opcodeInfo.IsValid()) {
            break;
        }

        const QString name = QString::fromLatin1(opcodeInfo.GetName("Unknown"));
        if (!name.contains(filter, Qt::CaseInsensitive)) {
            continue;
        }

        const std::uint32_t opcode = static_cast<std::uint32_t>(opcodeInfo.GetOpcode());
        (channelValues.push_back(opcode), ...);
    }
}

bool parseSearchIntegerFast(QString text, qulonglong& value)
{
    text = text.trimmed();
    if (text.isEmpty()) {
        return false;
    }

    bool ok = false;
    if (text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        value = text.sliced(2).toULongLong(&ok, 16);
        return ok;
    }

    bool hasHexLetter = false;
    for (const QChar ch : text) {
        if (ch.isDigit()) {
            continue;
        }

        const ushort code = ch.unicode();
        if ((code >= 'a' && code <= 'f') || (code >= 'A' && code <= 'F')) {
            hasHexLetter = true;
            continue;
        }

        return false;
    }

    value = text.toULongLong(&ok, hasHexLetter ? 16 : 10);
    return ok;
}

bool isIncompleteHexPrefix(QString text)
{
    text = text.trimmed();
    return text.compare(QStringLiteral("0x"), Qt::CaseInsensitive) == 0;
}

bool matchesFilterFast(const QString& value, const QString& filter)
{
    if (filter.isEmpty()) {
        return true;
    }

    if (isIncompleteHexPrefix(filter)) {
        return false;
    }

    qulonglong filterNumber = 0;
    qulonglong valueNumber = 0;
    const bool filterIsNumeric = parseSearchIntegerFast(filter, filterNumber);
    if (filterIsNumeric && parseSearchIntegerFast(value, valueNumber)) {
        return filterNumber == valueNumber;
    }

    return value.contains(filter, Qt::CaseInsensitive);
}

QString decimalValue(const qulonglong value)
{
    return QString::number(value);
}

QString hexValue(const qulonglong value)
{
    return QStringLiteral("0x%1").arg(QString::number(value, 16).toUpper());
}

QString optionalDecimalValue(const std::uint32_t value)
{
    return value == std::numeric_limits<std::uint32_t>::max()
        ? QString()
        : decimalValue(value);
}

QString optionalAddressValue(const std::uint64_t value, const size_t bitWidth)
{
    return value == std::numeric_limits<std::uint64_t>::max()
        ? QString()
        : formatFixedHex(value, bitWidth);
}

QString opcodeNameValue(const char* name)
{
    return QString::fromLatin1(name ? name : "Unknown");
}

QString fastOpcodeDisplayValue(const CLog::Parameters& params, const CLogBTraceFastRecordInfo& info)
{
    switch (params.GetIssue()) {
    case CLog::Issue::Eb:
        switch (static_cast<CLog::Channel>(info.channel)) {
        case CLog::Channel::TXREQ:
        case CLog::Channel::TXREQ_BeforeSAM:
        case CLog::Channel::RXREQ_BeforeSAM:
        case CLog::Channel::RXREQ:
            return opcodeNameValue(
                CHI::Eb::Opcodes::REQ::Decoder<FlexibleReqFlit>::INSTANCE
                    .Decode(static_cast<FlexibleReqFlit::opcode_t>(info.opcode))
                    .GetName("Unknown"));
        case CLog::Channel::TXRSP:
        case CLog::Channel::RXRSP:
            return opcodeNameValue(
                CHI::Eb::Opcodes::RSP::Decoder<FlexibleRspFlit>::INSTANCE
                    .Decode(static_cast<FlexibleRspFlit::opcode_t>(info.opcode))
                    .GetName("Unknown"));
        case CLog::Channel::TXDAT:
        case CLog::Channel::RXDAT:
            return opcodeNameValue(
                CHI::Eb::Opcodes::DAT::Decoder<FlexibleDatFlit>::INSTANCE
                    .Decode(static_cast<FlexibleDatFlit::opcode_t>(info.opcode))
                    .GetName("Unknown"));
        case CLog::Channel::TXSNP:
        case CLog::Channel::RXSNP:
            return opcodeNameValue(
                CHI::Eb::Opcodes::SNP::Decoder<FlexibleSnpFlit>::INSTANCE
                    .Decode(static_cast<FlexibleSnpFlit::opcode_t>(info.opcode))
                    .GetName("Unknown"));
        }
        break;
    case CLog::Issue::B:
        switch (static_cast<CLog::Channel>(info.channel)) {
        case CLog::Channel::TXREQ:
        case CLog::Channel::TXREQ_BeforeSAM:
        case CLog::Channel::RXREQ_BeforeSAM:
        case CLog::Channel::RXREQ:
            return opcodeNameValue(
                CHI::B::Opcodes::REQ::Decoder<FlexibleBReqFlit>::INSTANCE
                    .Decode(static_cast<FlexibleBReqFlit::opcode_t>(info.opcode))
                    .GetName("Unknown"));
        case CLog::Channel::TXRSP:
        case CLog::Channel::RXRSP:
            return opcodeNameValue(
                CHI::B::Opcodes::RSP::Decoder<FlexibleBRspFlit>::INSTANCE
                    .Decode(static_cast<FlexibleBRspFlit::opcode_t>(info.opcode))
                    .GetName("Unknown"));
        case CLog::Channel::TXDAT:
        case CLog::Channel::RXDAT:
            return opcodeNameValue(
                CHI::B::Opcodes::DAT::Decoder<FlexibleBDatFlit>::INSTANCE
                    .Decode(static_cast<FlexibleBDatFlit::opcode_t>(info.opcode))
                    .GetName("Unknown"));
        case CLog::Channel::TXSNP:
        case CLog::Channel::RXSNP:
            return opcodeNameValue(
                CHI::B::Opcodes::SNP::Decoder<FlexibleBSnpFlit>::INSTANCE
                    .Decode(static_cast<FlexibleBSnpFlit::opcode_t>(info.opcode))
                    .GetName("Unknown"));
        }
        break;
    }

    return QStringLiteral("Unknown");
}

bool fastRecordMatchesFilter(const CLog::Parameters& params,
                             const CLogBTraceFastFilter& filter,
                             const CLogBTraceFastRecordInfo& info)
{
    const CLogBTraceChannelMask channelBit =
        static_cast<CLogBTraceChannelMask>(CLogBTraceChannelMask{1} << info.channel);
    if ((filter.transportMask & channelBit) == 0) {
        return false;
    }

    const QString opcodeRaw = hexValue(info.opcode);
    if (!filter.opcodeFilter.isEmpty() && !matchesFilterFast(opcodeRaw, filter.opcodeFilter)) {
        bool opcodeMatched = false;
        const auto& opcodeValues =
            filter.opcodeValuesByChannel[static_cast<unsigned int>(info.channel)];
        if (!opcodeValues.empty()) {
            opcodeMatched = std::binary_search(opcodeValues.begin(),
                                               opcodeValues.end(),
                                               info.opcode);
        } else {
            opcodeMatched =
                fastOpcodeDisplayValue(params, info).contains(filter.opcodeFilter, Qt::CaseInsensitive);
        }

        if (!opcodeMatched) {
            return false;
        }
    }

    return matchesFilterFast(optionalDecimalValue(info.sourceId), filter.sourceIdFilter)
        && matchesFilterFast(optionalDecimalValue(info.targetId), filter.targetIdFilter)
        && matchesFilterFast(optionalDecimalValue(info.txnId), filter.txnIdFilter)
        && matchesFilterFast(optionalDecimalValue(info.dbid), filter.dbidFilter)
        && matchesFilterFast(optionalAddressValue(info.address, params.GetReqAddrWidth()),
                             filter.addressFilter);
}

bool extractFastRecordInfo(const CLog::Parameters& params,
                           const EbMeasures& measures,
                           const CLog::CLogB::TagCHIRecords::Record& source,
                           CLogBTraceFastRecordInfo& info)
{
    info = {};
    info.channel = static_cast<std::uint8_t>(source.channel);
    info.nodeId = static_cast<std::uint32_t>(source.nodeId);
    info.sourceId = std::numeric_limits<std::uint32_t>::max();
    info.targetId = std::numeric_limits<std::uint32_t>::max();
    info.txnId = std::numeric_limits<std::uint32_t>::max();
    info.dbid = std::numeric_limits<std::uint32_t>::max();
    info.address = std::numeric_limits<std::uint64_t>::max();

    const size_t bitLength = static_cast<size_t>(source.flitLength) * 8U;
    switch (source.channel) {
    case CLog::Channel::TXREQ:
    case CLog::Channel::TXREQ_BeforeSAM:
    case CLog::Channel::RXREQ_BeforeSAM:
    case CLog::Channel::RXREQ: {
        FlexibleReqFlit flit;
        if (!deserializeReq(measures.req, flit, source.flit.get(), bitLength)) {
            return false;
        }
        info.opcode = static_cast<std::uint32_t>(flit.Opcode());
        info.sourceId = static_cast<std::uint32_t>(flit.SrcID());
        info.targetId = static_cast<std::uint32_t>(flit.TgtID());
        info.txnId = static_cast<std::uint32_t>(flit.TxnID());
        info.address = static_cast<std::uint64_t>(flit.Addr());
        return true;
    }
    case CLog::Channel::TXRSP:
    case CLog::Channel::RXRSP: {
        FlexibleRspFlit flit;
        if (!deserializeRsp(measures.rsp, flit, source.flit.get(), bitLength)) {
            return false;
        }
        info.opcode = static_cast<std::uint32_t>(flit.Opcode());
        info.sourceId = static_cast<std::uint32_t>(flit.SrcID());
        info.targetId = static_cast<std::uint32_t>(flit.TgtID());
        info.txnId = static_cast<std::uint32_t>(flit.TxnID());
        info.dbid = static_cast<std::uint32_t>(flit.DBID());
        return true;
    }
    case CLog::Channel::TXDAT:
    case CLog::Channel::RXDAT: {
        FlexibleDatFlit flit;
        if (!deserializeDat(measures.dat, flit, source.flit.get(), bitLength)) {
            return false;
        }
        info.opcode = static_cast<std::uint32_t>(flit.Opcode());
        info.sourceId = static_cast<std::uint32_t>(flit.SrcID());
        info.targetId = static_cast<std::uint32_t>(flit.TgtID());
        info.txnId = static_cast<std::uint32_t>(flit.TxnID());
        info.dbid = static_cast<std::uint32_t>(flit.DBID());
        return true;
    }
    case CLog::Channel::TXSNP:
    case CLog::Channel::RXSNP: {
        FlexibleSnpFlit flit;
        if (!deserializeSnp(measures.snp, flit, source.flit.get(), bitLength)) {
            return false;
        }
        info.opcode = static_cast<std::uint32_t>(flit.Opcode());
        info.sourceId = static_cast<std::uint32_t>(flit.SrcID());
        info.txnId = static_cast<std::uint32_t>(flit.TxnID());
        info.address = static_cast<std::uint64_t>(flit.Addr() << 3);
        return true;
    }
    }

    return false;
}

bool extractFastRecordInfo(const CLog::Parameters& params,
                           const BMeasures& measures,
                           const CLog::CLogB::TagCHIRecords::Record& source,
                           CLogBTraceFastRecordInfo& info)
{
    info = {};
    info.channel = static_cast<std::uint8_t>(source.channel);
    info.nodeId = static_cast<std::uint32_t>(source.nodeId);
    info.sourceId = std::numeric_limits<std::uint32_t>::max();
    info.targetId = std::numeric_limits<std::uint32_t>::max();
    info.txnId = std::numeric_limits<std::uint32_t>::max();
    info.dbid = std::numeric_limits<std::uint32_t>::max();
    info.address = std::numeric_limits<std::uint64_t>::max();

    const size_t bitLength = static_cast<size_t>(source.flitLength) * 8U;
    switch (source.channel) {
    case CLog::Channel::TXREQ:
    case CLog::Channel::TXREQ_BeforeSAM:
    case CLog::Channel::RXREQ_BeforeSAM:
    case CLog::Channel::RXREQ: {
        FlexibleBReqFlit flit;
        if (!deserializeBReq(measures.req, flit, source.flit.get(), bitLength)) {
            return false;
        }
        info.opcode = static_cast<std::uint32_t>(flit.Opcode());
        info.sourceId = static_cast<std::uint32_t>(flit.SrcID());
        info.targetId = static_cast<std::uint32_t>(flit.TgtID());
        info.txnId = static_cast<std::uint32_t>(flit.TxnID());
        info.address = static_cast<std::uint64_t>(flit.Addr());
        return true;
    }
    case CLog::Channel::TXRSP:
    case CLog::Channel::RXRSP: {
        FlexibleBRspFlit flit;
        if (!deserializeBRsp(measures.rsp, flit, source.flit.get(), bitLength)) {
            return false;
        }
        info.opcode = static_cast<std::uint32_t>(flit.Opcode());
        info.sourceId = static_cast<std::uint32_t>(flit.SrcID());
        info.targetId = static_cast<std::uint32_t>(flit.TgtID());
        info.txnId = static_cast<std::uint32_t>(flit.TxnID());
        info.dbid = static_cast<std::uint32_t>(flit.DBID());
        return true;
    }
    case CLog::Channel::TXDAT:
    case CLog::Channel::RXDAT: {
        FlexibleBDatFlit flit;
        if (!deserializeBDat(measures.dat, flit, source.flit.get(), bitLength)) {
            return false;
        }
        info.opcode = static_cast<std::uint32_t>(flit.Opcode());
        info.sourceId = static_cast<std::uint32_t>(flit.SrcID());
        info.targetId = static_cast<std::uint32_t>(flit.TgtID());
        info.txnId = static_cast<std::uint32_t>(flit.TxnID());
        info.dbid = static_cast<std::uint32_t>(flit.DBID());
        return true;
    }
    case CLog::Channel::TXSNP:
    case CLog::Channel::RXSNP: {
        FlexibleBSnpFlit flit;
        if (!deserializeBSnp(measures.snp, flit, source.flit.get(), bitLength)) {
            return false;
        }
        info.opcode = static_cast<std::uint32_t>(flit.Opcode());
        info.sourceId = static_cast<std::uint32_t>(flit.SrcID());
        info.txnId = static_cast<std::uint32_t>(flit.TxnID());
        info.address = static_cast<std::uint64_t>(flit.Addr() << 3);
        return true;
    }
    }

    return false;
}

bool recordMatchesRawFastFilter(const CLogBTraceFastFilter& filter,
                                const CLog::Parameters& params,
                                const EbMeasures& measures,
                                const CLog::CLogB::TagCHIRecords::Record& source)
{
    CLogBTraceFastRecordInfo info;
    return extractFastRecordInfo(params, measures, source, info)
        && fastRecordMatchesFilter(params, filter, info);
}

bool recordMatchesRawFastFilter(const CLogBTraceFastFilter& filter,
                                const CLog::Parameters& params,
                                const BMeasures& measures,
                                const CLog::CLogB::TagCHIRecords::Record& source)
{
    CLogBTraceFastRecordInfo info;
    return extractFastRecordInfo(params, measures, source, info)
        && fastRecordMatchesFilter(params, filter, info);
}

bool bReqFieldCanAppear(const OptionalFieldId fieldId) noexcept
{
    switch (fieldId) {
    case OptionalFieldId::Opcode:
    case OptionalFieldId::SrcID:
    case OptionalFieldId::TgtID:
    case OptionalFieldId::TxnID:
    case OptionalFieldId::ReturnNID:
    case OptionalFieldId::StashNIDValid:
    case OptionalFieldId::ReturnTxnID:
    case OptionalFieldId::Size:
    case OptionalFieldId::Addr:
    case OptionalFieldId::NS:
    case OptionalFieldId::LikelyShared:
    case OptionalFieldId::AllowRetry:
    case OptionalFieldId::Order:
    case OptionalFieldId::PCrdType:
    case OptionalFieldId::MemAttr:
    case OptionalFieldId::SnpAttr:
    case OptionalFieldId::LPID:
    case OptionalFieldId::Excl:
    case OptionalFieldId::ExpCompAck:
    case OptionalFieldId::TraceTag:
    case OptionalFieldId::RSVDC:
        return true;
    default:
        return false;
    }
}

bool bRspFieldCanAppear(const OptionalFieldId fieldId) noexcept
{
    switch (fieldId) {
    case OptionalFieldId::Opcode:
    case OptionalFieldId::SrcID:
    case OptionalFieldId::TgtID:
    case OptionalFieldId::TxnID:
    case OptionalFieldId::RespErr:
    case OptionalFieldId::Resp:
    case OptionalFieldId::FwdState:
    case OptionalFieldId::DBID:
    case OptionalFieldId::PCrdType:
    case OptionalFieldId::TraceTag:
        return true;
    default:
        return false;
    }
}

bool bDatFieldCanAppear(const OptionalFieldId fieldId) noexcept
{
    switch (fieldId) {
    case OptionalFieldId::Opcode:
    case OptionalFieldId::SrcID:
    case OptionalFieldId::TgtID:
    case OptionalFieldId::TxnID:
    case OptionalFieldId::HomeNID:
    case OptionalFieldId::RespErr:
    case OptionalFieldId::Resp:
    case OptionalFieldId::FwdState:
    case OptionalFieldId::DataSource:
    case OptionalFieldId::DBID:
    case OptionalFieldId::CCID:
    case OptionalFieldId::DataID:
    case OptionalFieldId::TraceTag:
    case OptionalFieldId::RSVDC:
    case OptionalFieldId::BE:
    case OptionalFieldId::Data:
    case OptionalFieldId::DataCheck:
    case OptionalFieldId::Poison:
        return true;
    default:
        return false;
    }
}

bool bSnpFieldCanAppear(const OptionalFieldId fieldId) noexcept
{
    switch (fieldId) {
    case OptionalFieldId::Opcode:
    case OptionalFieldId::SrcID:
    case OptionalFieldId::TxnID:
    case OptionalFieldId::FwdNID:
    case OptionalFieldId::FwdTxnID:
    case OptionalFieldId::Addr:
    case OptionalFieldId::NS:
    case OptionalFieldId::DoNotGoToSD:
    case OptionalFieldId::RetToSrc:
    case OptionalFieldId::TraceTag:
        return true;
    default:
        return false;
    }
}

bool bFieldCanAppear(const OptionalFieldId fieldId) noexcept
{
    return bReqFieldCanAppear(fieldId)
        || bRspFieldCanAppear(fieldId)
        || bDatFieldCanAppear(fieldId)
        || bSnpFieldCanAppear(fieldId);
}

bool buildEbOptionalFieldRecords(const CLog::Parameters& params,
                                 const EbMeasures& measures,
                                 const CLog::CLogB::TagCHIRecords& block,
                                 const QString& fieldName,
                                 std::vector<CLogBTraceOptionalFieldValue>& records,
                                 CLogBTraceLoadError& error,
                                 const std::size_t maxWorkerCount,
                                 const CLogBTraceLoadStageProgressCallback& progressCallback,
                                 std::stop_token stopToken)
{
    records.assign(block.records.size(), CLogBTraceOptionalFieldValue{});

    const EbOptionalFieldKeys keys = ebOptionalFieldKeysForName(fieldName);
    if (!hasAnyEbOptionalFieldKey(keys)) {
        if (progressCallback) {
            progressCallback(records.size(), records.size());
        }
        return true;
    }

    std::vector<qulonglong> recordTimestamps;
    std::size_t failedRecordIndex = 0;
    if (!buildRecordTimestamps(block, recordTimestamps, failedRecordIndex)) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("Record %1 timestamp exceeds the GUI timestamp range.")
                         .arg(static_cast<qulonglong>(failedRecordIndex + 1)));
        records.clear();
        return false;
    }

    const QString flitReport = buildFlitReport(params, measures);
    const OptionalFieldId fieldId = optionalFieldIdForName(fieldName);
    const auto decodeRecord = [&](const std::size_t recordIndex, CLogBTraceLoadError& workerError) -> bool {
        const CLog::CLogB::TagCHIRecords::Record& source = block.records[recordIndex];
        const size_t bitLength = static_cast<size_t>(source.flitLength) * 8U;
        CLogBTraceOptionalFieldValue value;

        switch (source.channel) {
        case CLog::Channel::TXREQ:
        case CLog::Channel::TXREQ_BeforeSAM:
        case CLog::Channel::RXREQ:
        case CLog::Channel::RXREQ_BeforeSAM: {
            if (!keys.req) {
                return true;
            }

            FlexibleReqFlit flit;
            if (!deserializeReq(measures.req, flit, source.flit.get(), bitLength)) {
                setDeserializationLoadError(workerError,
                                            params,
                                            QStringLiteral("E.b"),
                                            QStringLiteral("REQ"),
                                            block,
                                            source,
                                            recordIndex,
                                            recordTimestamps[recordIndex],
                                            measures.req,
                                            FlexibleReqFlit::WIDTH);
                workerError.flitReportText = flitReport;
                return false;
            }
            extractEbReqOptionalField(params, fieldId, keys.req, flit, value);
            break;
        }
        case CLog::Channel::TXRSP:
        case CLog::Channel::RXRSP: {
            if (!keys.rsp) {
                return true;
            }

            FlexibleRspFlit flit;
            if (!deserializeRsp(measures.rsp, flit, source.flit.get(), bitLength)) {
                setDeserializationLoadError(workerError,
                                            params,
                                            QStringLiteral("E.b"),
                                            QStringLiteral("RSP"),
                                            block,
                                            source,
                                            recordIndex,
                                            recordTimestamps[recordIndex],
                                            measures.rsp,
                                            FlexibleRspFlit::WIDTH);
                workerError.flitReportText = flitReport;
                return false;
            }
            extractEbRspOptionalField(keys.rsp, fieldId, flit, value);
            break;
        }
        case CLog::Channel::TXDAT:
        case CLog::Channel::RXDAT: {
            if (!keys.dat) {
                return true;
            }

            FlexibleDatFlit flit;
            if (!deserializeDat(measures.dat, flit, source.flit.get(), bitLength)) {
                setDeserializationLoadError(workerError,
                                            params,
                                            QStringLiteral("E.b"),
                                            QStringLiteral("DAT"),
                                            block,
                                            source,
                                            recordIndex,
                                            recordTimestamps[recordIndex],
                                            measures.dat,
                                            FlexibleDatFlit::WIDTH);
                workerError.flitReportText = flitReport;
                return false;
            }
            extractEbDatOptionalField(params, fieldId, keys.dat, flit, value);
            break;
        }
        case CLog::Channel::TXSNP:
        case CLog::Channel::RXSNP: {
            if (!keys.snp) {
                return true;
            }

            FlexibleSnpFlit flit;
            if (!deserializeSnp(measures.snp, flit, source.flit.get(), bitLength)) {
                setDeserializationLoadError(workerError,
                                            params,
                                            QStringLiteral("E.b"),
                                            QStringLiteral("SNP"),
                                            block,
                                            source,
                                            recordIndex,
                                            recordTimestamps[recordIndex],
                                            measures.snp,
                                            FlexibleSnpFlit::WIDTH);
                workerError.flitReportText = flitReport;
                return false;
            }
            extractEbSnpOptionalField(params, fieldId, keys.snp, flit, value);
            break;
        }
        }

        records[recordIndex] = std::move(value);
        return true;
    };

    if (!decodeRecords(block.records.size(),
                       decodeRecord,
                       error,
                       progressCallback,
                       {},
                       stopToken,
                       maxWorkerCount > 1,
                       maxWorkerCount)) {
        records.clear();
        return false;
    }

    return true;
}

bool buildBOptionalFieldRecords(const CLog::Parameters& params,
                                const BMeasures& measures,
                                const CLog::CLogB::TagCHIRecords& block,
                                const QString& fieldName,
                                std::vector<CLogBTraceOptionalFieldValue>& records,
                                CLogBTraceLoadError& error,
                                const std::size_t maxWorkerCount,
                                const CLogBTraceLoadStageProgressCallback& progressCallback,
                                std::stop_token stopToken)
{
    records.assign(block.records.size(), CLogBTraceOptionalFieldValue{});

    const OptionalFieldId fieldId = optionalFieldIdForName(fieldName);
    if (!bFieldCanAppear(fieldId)) {
        if (progressCallback) {
            progressCallback(records.size(), records.size());
        }
        return true;
    }

    std::vector<qulonglong> recordTimestamps;
    std::size_t failedRecordIndex = 0;
    if (!buildRecordTimestamps(block, recordTimestamps, failedRecordIndex)) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("Record %1 timestamp exceeds the GUI timestamp range.")
                         .arg(static_cast<qulonglong>(failedRecordIndex + 1)));
        records.clear();
        return false;
    }

    const QString flitReport = buildFlitReport(params, measures);
    const auto decodeRecord = [&](const std::size_t recordIndex, CLogBTraceLoadError& workerError) -> bool {
        const CLog::CLogB::TagCHIRecords::Record& source = block.records[recordIndex];
        const size_t bitLength = static_cast<size_t>(source.flitLength) * 8U;
        CLogBTraceOptionalFieldValue value;

        switch (source.channel) {
        case CLog::Channel::TXREQ:
        case CLog::Channel::TXREQ_BeforeSAM:
        case CLog::Channel::RXREQ_BeforeSAM:
        case CLog::Channel::RXREQ: {
            if (!bReqFieldCanAppear(fieldId)) {
                return true;
            }

            FlexibleBReqFlit flit;
            if (!deserializeBReq(measures.req, flit, source.flit.get(), bitLength)) {
                setDeserializationLoadError(workerError,
                                            params,
                                            QStringLiteral("B"),
                                            QStringLiteral("REQ"),
                                            block,
                                            source,
                                            recordIndex,
                                            recordTimestamps[recordIndex],
                                            measures.req,
                                            FlexibleBReqFlit::WIDTH);
                workerError.flitReportText = flitReport;
                return false;
            }
            extractBReqOptionalField(params, fieldId, flit, value);
            break;
        }
        case CLog::Channel::TXRSP:
        case CLog::Channel::RXRSP: {
            if (!bRspFieldCanAppear(fieldId)) {
                return true;
            }

            FlexibleBRspFlit flit;
            if (!deserializeBRsp(measures.rsp, flit, source.flit.get(), bitLength)) {
                setDeserializationLoadError(workerError,
                                            params,
                                            QStringLiteral("B"),
                                            QStringLiteral("RSP"),
                                            block,
                                            source,
                                            recordIndex,
                                            recordTimestamps[recordIndex],
                                            measures.rsp,
                                            FlexibleBRspFlit::WIDTH);
                workerError.flitReportText = flitReport;
                return false;
            }
            extractBRspOptionalField(fieldId, flit, value);
            break;
        }
        case CLog::Channel::TXDAT:
        case CLog::Channel::RXDAT: {
            if (!bDatFieldCanAppear(fieldId)) {
                return true;
            }

            FlexibleBDatFlit flit;
            if (!deserializeBDat(measures.dat, flit, source.flit.get(), bitLength)) {
                setDeserializationLoadError(workerError,
                                            params,
                                            QStringLiteral("B"),
                                            QStringLiteral("DAT"),
                                            block,
                                            source,
                                            recordIndex,
                                            recordTimestamps[recordIndex],
                                            measures.dat,
                                            FlexibleBDatFlit::WIDTH);
                workerError.flitReportText = flitReport;
                return false;
            }
            extractBDatOptionalField(params, fieldId, flit, value);
            break;
        }
        case CLog::Channel::TXSNP:
        case CLog::Channel::RXSNP: {
            if (!bSnpFieldCanAppear(fieldId)) {
                return true;
            }

            FlexibleBSnpFlit flit;
            if (!deserializeBSnp(measures.snp, flit, source.flit.get(), bitLength)) {
                setDeserializationLoadError(workerError,
                                            params,
                                            QStringLiteral("B"),
                                            QStringLiteral("SNP"),
                                            block,
                                            source,
                                            recordIndex,
                                            recordTimestamps[recordIndex],
                                            measures.snp,
                                            FlexibleBSnpFlit::WIDTH);
                workerError.flitReportText = flitReport;
                return false;
            }
            extractBSnpOptionalField(params, fieldId, flit, value);
            break;
        }
        }

        records[recordIndex] = std::move(value);
        return true;
    };

    if (!decodeRecords(block.records.size(),
                       decodeRecord,
                       error,
                       progressCallback,
                       {},
                       stopToken,
                       maxWorkerCount > 1,
                       maxWorkerCount)) {
        records.clear();
        return false;
    }

    return true;
}

QString resolveTraceFilePath(const QString& filePath,
                             const CLogBTraceMetadata& metadata)
{
    return filePath.isEmpty() ? metadata.filePath : filePath;
}

template<typename Config>
void storeEbXactionIndexRecord(
    CLogBTraceXactionIndexRecord& indexRecord,
    const EbXactionProcessResult<Config>& result)
{
    indexRecord = {};
    indexRecord.processed = true;
    indexRecord.processedByJoint = true;

    if (result.denial == CHI::Eb::Xact::XactDenial::ACCEPTED && result.ordinal) {
        indexRecord.indexed = true;
        indexRecord.xactionComplete = result.xactionComplete;
        indexRecord.xactionOrdinal = *result.ordinal;
        indexRecord.transactionGroupKey =
            QStringLiteral("xaction|%1").arg(static_cast<qulonglong>(*result.ordinal));
    }

    indexRecord.jointType = result.jointType;
    indexRecord.jointPath = result.path;
    indexRecord.denialName = ebXactionDenialName(result.denial);
    indexRecord.denialCode = ebXactionDenialCode(result.denial);
    indexRecord.xactionType = result.xaction
        ? ebXactionTypeName(result.xaction->GetType())
        : QStringLiteral("none");
}

void storeEbXactionNotIndexedRecord(CLogBTraceXactionIndexRecord& indexRecord)
{
    indexRecord = {};
    indexRecord.processed = true;
    indexRecord.processedByJoint = false;
    indexRecord.jointPath = QStringLiteral("not processed");
    indexRecord.denialName = QStringLiteral("not processed");
    indexRecord.denialCode = QStringLiteral("-");
    indexRecord.xactionType = QStringLiteral("none");
}

inline constexpr std::size_t kXactionThreadsPerRecordChunk = 2;
inline constexpr std::size_t kXactionGlobalThreadLimit = 16;
inline constexpr std::size_t kXactionMergeProgressBatchSize = 2048;
inline constexpr std::size_t kNoXactionContext =
    (std::numeric_limits<std::size_t>::max)();

template<typename Config>
struct EbXactionIndexThreadState {
    CHI::Eb::Xact::Global<Config> xactGlobal;
    CHI::Eb::Xact::RNFJoint<Config> rnJoint;
    CHI::Eb::Xact::SNFJoint<Config> snJoint;
    std::uint64_t nextXactionOrdinal = 1;
};

template<typename Config>
struct EbXactionIndexSliceContext {
    std::size_t contextIndex = 0;
    std::size_t blockIndex = 0;
    std::size_t localBegin = 0;
    std::size_t localEnd = 0;
    std::uint64_t logicalBegin = 0;
    std::uint64_t logicalEnd = 0;
    EbXactionIndexThreadState<Config> state;
};

struct PendingEbXactionIndexRecord {
    CLogBTraceXactionIndexRecord record;
    std::size_t ownerContext = kNoXactionContext;
    std::uint64_t ownerOrdinal = 0;
};

struct DeniedEbXactionIndexFlit {
    std::uint64_t logicalRow = 0;
    std::size_t sourceContext = kNoXactionContext;
    DecodedEbRecord decodedRecord;
};

struct EbXactionIndexBlockCacheEntry {
    std::mutex mutex;
    std::condition_variable readyCondition;
    std::shared_ptr<const CLog::CLogB::TagCHIRecords> block;
    std::shared_ptr<const std::vector<qulonglong>> recordTimestamps;
    CLogBTraceLoadError error;
    std::atomic<std::size_t> remainingContexts = 0;
    bool loading = false;
    bool ready = false;
    bool failed = false;
};

template<typename Config>
void initializeEbXactionIndexState(
    EbXactionIndexThreadState<Config>& state,
    const std::unordered_map<quint16, CLog::NodeType>& nodeTypes)
{
    populateEbXactTopology(state.xactGlobal.TOPOLOGY, nodeTypes);
    state.nextXactionOrdinal = 1;
}

void setPendingEbXactionRecordOwner(PendingEbXactionIndexRecord& pending,
                                    const std::size_t ownerContext)
{
    if (pending.record.indexed && pending.record.xactionOrdinal != 0) {
        pending.ownerContext = ownerContext;
        pending.ownerOrdinal = pending.record.xactionOrdinal;
        return;
    }

    pending.ownerContext = kNoXactionContext;
    pending.ownerOrdinal = 0;
}

template<typename Config>
bool processEbXactionIndexSourceRecord(
    const CLogBTraceMetadata& metadata,
    const EbMeasures& measures,
    const QString& flitReport,
    const CLog::CLogB::TagCHIRecords& block,
    const CLog::CLogB::TagCHIRecords::Record& source,
    const std::size_t localIndex,
    const qulonglong timestamp,
    EbXactionIndexThreadState<Config>& state,
    CLogBTraceXactionIndexRecord& indexRecord,
    DecodedEbRecord* decodedRecordOut,
    bool* deniedByJointOut,
    CLogBTraceLoadError& error)
{
    const qint64 decodedTimestamp = static_cast<qint64>(timestamp);
    const size_t bitLength = static_cast<size_t>(source.flitLength) * 8U;
    DecodedEbRecord decodedRecord;
    decodedRecord.timestamp = decodedTimestamp;
    decodedRecord.direction = isTxChannel(source.channel) ? FlitDirection::Tx
                                                          : FlitDirection::Rx;
    decodedRecord.nodeId = source.nodeId;
    decodedRecord.sourceChannel = source.channel;

    bool deniedByJoint = false;
    auto storeResult = [&](const auto& result) {
        storeEbXactionIndexRecord(indexRecord, result);
        deniedByJoint = result.denial
            && result.denial != CHI::Eb::Xact::XactDenial::ACCEPTED;
    };

    switch (source.channel) {
    case CLog::Channel::TXREQ:
    case CLog::Channel::TXREQ_BeforeSAM:
    case CLog::Channel::RXREQ:
    case CLog::Channel::RXREQ_BeforeSAM: {
        FlexibleReqFlit flit;
        if (!deserializeReq(measures.req, flit, source.flit.get(), bitLength)) {
            setDeserializationLoadError(error,
                                        metadata.parameters,
                                        QStringLiteral("E.b"),
                                        QStringLiteral("REQ"),
                                        block,
                                        source,
                                        localIndex,
                                        timestamp,
                                        measures.req,
                                        FlexibleReqFlit::WIDTH);
            error.flitReportText = flitReport;
            return false;
        }

        decodedRecord.flit = flit;
        const auto xactionFlit = makeEbXactionReqFlit<Config>(flit);
        storeResult(processEbReqXactionByRecordNode(state.xactGlobal,
                                                    state.xactGlobal.TOPOLOGY,
                                                    state.rnJoint,
                                                    state.snJoint,
                                                    metadata.nodeTypes,
                                                    decodedRecord,
                                                    xactionFlit,
                                                    state.nextXactionOrdinal));
        break;
    }
    case CLog::Channel::TXRSP:
    case CLog::Channel::RXRSP: {
        FlexibleRspFlit flit;
        if (!deserializeRsp(measures.rsp, flit, source.flit.get(), bitLength)) {
            setDeserializationLoadError(error,
                                        metadata.parameters,
                                        QStringLiteral("E.b"),
                                        QStringLiteral("RSP"),
                                        block,
                                        source,
                                        localIndex,
                                        timestamp,
                                        measures.rsp,
                                        FlexibleRspFlit::WIDTH);
            error.flitReportText = flitReport;
            return false;
        }

        decodedRecord.flit = flit;
        const auto xactionFlit = makeEbXactionRspFlit<Config>(flit);
        storeResult(processEbRspXactionByRecordNode(state.xactGlobal,
                                                    state.xactGlobal.TOPOLOGY,
                                                    state.rnJoint,
                                                    state.snJoint,
                                                    metadata.nodeTypes,
                                                    decodedRecord,
                                                    xactionFlit,
                                                    state.nextXactionOrdinal));
        break;
    }
    case CLog::Channel::TXDAT:
    case CLog::Channel::RXDAT: {
        FlexibleDatFlit flit;
        if (!deserializeDat(measures.dat, flit, source.flit.get(), bitLength)) {
            setDeserializationLoadError(error,
                                        metadata.parameters,
                                        QStringLiteral("E.b"),
                                        QStringLiteral("DAT"),
                                        block,
                                        source,
                                        localIndex,
                                        timestamp,
                                        measures.dat,
                                        FlexibleDatFlit::WIDTH);
            error.flitReportText = flitReport;
            return false;
        }

        decodedRecord.flit = flit;
        const auto xactionFlit = makeEbXactionDatFlit<Config>(flit);
        storeResult(processEbDatXactionByRecordNode(state.xactGlobal,
                                                    state.xactGlobal.TOPOLOGY,
                                                    state.rnJoint,
                                                    state.snJoint,
                                                    metadata.nodeTypes,
                                                    decodedRecord,
                                                    xactionFlit,
                                                    state.nextXactionOrdinal));
        break;
    }
    case CLog::Channel::TXSNP:
    case CLog::Channel::RXSNP: {
        FlexibleSnpFlit flit;
        if (!deserializeSnp(measures.snp, flit, source.flit.get(), bitLength)) {
            setDeserializationLoadError(error,
                                        metadata.parameters,
                                        QStringLiteral("E.b"),
                                        QStringLiteral("SNP"),
                                        block,
                                        source,
                                        localIndex,
                                        timestamp,
                                        measures.snp,
                                        FlexibleSnpFlit::WIDTH);
            error.flitReportText = flitReport;
            return false;
        }

        decodedRecord.flit = flit;
        const auto xactionFlit = makeEbXactionSnpFlit<Config>(flit);
        storeResult(processEbSnpXactionByRecordNode(state.xactGlobal,
                                                    state.xactGlobal.TOPOLOGY,
                                                    state.rnJoint,
                                                    state.snJoint,
                                                    metadata.nodeTypes,
                                                    decodedRecord,
                                                    xactionFlit,
                                                    state.nextXactionOrdinal));
        break;
    }
    default:
        storeEbXactionNotIndexedRecord(indexRecord);
        break;
    }

    if (decodedRecordOut) {
        *decodedRecordOut = std::move(decodedRecord);
    }
    if (deniedByJointOut) {
        *deniedByJointOut = deniedByJoint;
    }
    return true;
}

template<typename Config>
struct EbCacheReplayRecord {
    DecodedEbRecord decodedRecord;
    EbXactionProcessResult<Config> result;
    bool processed = false;
};

template<typename Config>
bool processEbCacheReplaySourceRecord(
    const CLogBTraceMetadata& metadata,
    const EbMeasures& measures,
    const QString& flitReport,
    const CLog::CLogB::TagCHIRecords& block,
    const CLog::CLogB::TagCHIRecords::Record& source,
    const std::size_t localIndex,
    const qulonglong timestamp,
    EbXactionIndexThreadState<Config>& state,
    EbCacheReplayRecord<Config>& replayRecord,
    CLogBTraceLoadError& error)
{
    const qint64 decodedTimestamp = static_cast<qint64>(timestamp);
    const size_t bitLength = static_cast<size_t>(source.flitLength) * 8U;
    DecodedEbRecord decodedRecord;
    decodedRecord.timestamp = decodedTimestamp;
    decodedRecord.direction = isTxChannel(source.channel) ? FlitDirection::Tx
                                                          : FlitDirection::Rx;
    decodedRecord.nodeId = source.nodeId;
    decodedRecord.sourceChannel = source.channel;

    replayRecord = {};

    switch (source.channel) {
    case CLog::Channel::TXREQ:
    case CLog::Channel::TXREQ_BeforeSAM:
    case CLog::Channel::RXREQ:
    case CLog::Channel::RXREQ_BeforeSAM: {
        FlexibleReqFlit flit;
        if (!deserializeReq(measures.req, flit, source.flit.get(), bitLength)) {
            setDeserializationLoadError(error,
                                        metadata.parameters,
                                        QStringLiteral("E.b"),
                                        QStringLiteral("REQ"),
                                        block,
                                        source,
                                        localIndex,
                                        timestamp,
                                        measures.req,
                                        FlexibleReqFlit::WIDTH);
            error.flitReportText = flitReport;
            return false;
        }

        decodedRecord.flit = flit;
        const auto xactionFlit = makeEbXactionReqFlit<Config>(flit);
        replayRecord.result = processEbReqXactionByRecordNode(state.xactGlobal,
                                                              state.xactGlobal.TOPOLOGY,
                                                              state.rnJoint,
                                                              state.snJoint,
                                                              metadata.nodeTypes,
                                                              decodedRecord,
                                                              xactionFlit,
                                                              state.nextXactionOrdinal);
        replayRecord.processed = true;
        break;
    }
    case CLog::Channel::TXRSP:
    case CLog::Channel::RXRSP: {
        FlexibleRspFlit flit;
        if (!deserializeRsp(measures.rsp, flit, source.flit.get(), bitLength)) {
            setDeserializationLoadError(error,
                                        metadata.parameters,
                                        QStringLiteral("E.b"),
                                        QStringLiteral("RSP"),
                                        block,
                                        source,
                                        localIndex,
                                        timestamp,
                                        measures.rsp,
                                        FlexibleRspFlit::WIDTH);
            error.flitReportText = flitReport;
            return false;
        }

        decodedRecord.flit = flit;
        const auto xactionFlit = makeEbXactionRspFlit<Config>(flit);
        replayRecord.result = processEbRspXactionByRecordNode(state.xactGlobal,
                                                              state.xactGlobal.TOPOLOGY,
                                                              state.rnJoint,
                                                              state.snJoint,
                                                              metadata.nodeTypes,
                                                              decodedRecord,
                                                              xactionFlit,
                                                              state.nextXactionOrdinal);
        replayRecord.processed = true;
        break;
    }
    case CLog::Channel::TXDAT:
    case CLog::Channel::RXDAT: {
        FlexibleDatFlit flit;
        if (!deserializeDat(measures.dat, flit, source.flit.get(), bitLength)) {
            setDeserializationLoadError(error,
                                        metadata.parameters,
                                        QStringLiteral("E.b"),
                                        QStringLiteral("DAT"),
                                        block,
                                        source,
                                        localIndex,
                                        timestamp,
                                        measures.dat,
                                        FlexibleDatFlit::WIDTH);
            error.flitReportText = flitReport;
            return false;
        }

        decodedRecord.flit = flit;
        const auto xactionFlit = makeEbXactionDatFlit<Config>(flit);
        replayRecord.result = processEbDatXactionByRecordNode(state.xactGlobal,
                                                              state.xactGlobal.TOPOLOGY,
                                                              state.rnJoint,
                                                              state.snJoint,
                                                              metadata.nodeTypes,
                                                              decodedRecord,
                                                              xactionFlit,
                                                              state.nextXactionOrdinal);
        replayRecord.processed = true;
        break;
    }
    case CLog::Channel::TXSNP:
    case CLog::Channel::RXSNP: {
        FlexibleSnpFlit flit;
        if (!deserializeSnp(measures.snp, flit, source.flit.get(), bitLength)) {
            setDeserializationLoadError(error,
                                        metadata.parameters,
                                        QStringLiteral("E.b"),
                                        QStringLiteral("SNP"),
                                        block,
                                        source,
                                        localIndex,
                                        timestamp,
                                        measures.snp,
                                        FlexibleSnpFlit::WIDTH);
            error.flitReportText = flitReport;
            return false;
        }

        decodedRecord.flit = flit;
        const auto xactionFlit = makeEbXactionSnpFlit<Config>(flit);
        replayRecord.result = processEbSnpXactionByRecordNode(state.xactGlobal,
                                                              state.xactGlobal.TOPOLOGY,
                                                              state.rnJoint,
                                                              state.snJoint,
                                                              metadata.nodeTypes,
                                                              decodedRecord,
                                                              xactionFlit,
                                                              state.nextXactionOrdinal);
        replayRecord.processed = true;
        break;
    }
    default:
        break;
    }

    replayRecord.decodedRecord = std::move(decodedRecord);
    return true;
}

inline constexpr std::uint64_t kCacheLineAddressMask = ~std::uint64_t{0x3f};

std::uint64_t normalizedCacheLineAddress(const std::uint64_t address) noexcept
{
    return address & kCacheLineAddressMask;
}

std::uint8_t cacheStateMask(const CHI::Eb::Xact::CacheState state) noexcept
{
    return state.i8 & 0x7fU;
}

bool cacheStateAlive(const CHI::Eb::Xact::CacheState state) noexcept
{
    return (cacheStateMask(state) & 0x3fU) != 0U;
}

QString cacheStateText(const CHI::Eb::Xact::CacheState state)
{
    const std::string text = state.ToString();
    return text.empty() ? QStringLiteral("None")
                        : QString::fromStdString(text);
}

struct CacheLineKey {
    std::uint32_t rnNodeId = 0;
    std::uint64_t lineAddress = 0;

    bool operator==(const CacheLineKey& other) const noexcept
    {
        return rnNodeId == other.rnNodeId && lineAddress == other.lineAddress;
    }
};

struct CacheLineKeyHash {
    std::size_t operator()(const CacheLineKey& key) const noexcept
    {
        const std::uint64_t mixed =
            key.lineAddress ^ (static_cast<std::uint64_t>(key.rnNodeId) * 0x9E3779B185EBCA87ULL);
        return static_cast<std::size_t>(mixed ^ (mixed >> 32U));
    }
};

struct ActiveCacheLineLifetime {
    qint64 startTimestamp = 0;
    std::uint64_t startLogicalRow = 0;
    std::uint8_t startStateMask = 0;
    QString startStateText;
    std::uint8_t lastStateMask = 0;
    QString lastStateText;
};

template<typename Config>
std::uint64_t xactionFirstAddress(const CHI::Eb::Xact::Xaction<Config>& xaction) noexcept
{
    const auto& first = xaction.GetFirst();
    if (first.IsREQ()) {
        return static_cast<std::uint64_t>(first.flit.req.Addr());
    }
    return static_cast<std::uint64_t>(first.flit.snp.Addr()) << 3U;
}

template<typename Config>
class EbCacheLifetimeReplay final {
public:
    explicit EbCacheLifetimeReplay(const CLogBTraceCacheLineLifetimeCallback& callback)
        : callback_(callback)
    {
    }

    bool replay(const CLogBTraceMetadata& metadata,
                const EbCacheReplayRecord<Config>& replayRecord,
                const std::uint64_t logicalRow,
                CLogBTraceLoadError& error)
    {
        const DecodedEbRecord& decodedRecord = replayRecord.decodedRecord;
        const auto nodeType = ebXactionNodeTypeForRecord(metadata.nodeTypes, decodedRecord);
        if (!nodeType || !ebXactionUseRnfJoint(*nodeType)) {
            return true;
        }
        if (!replayRecord.processed
            || replayRecord.result.denial != CHI::Eb::Xact::XactDenial::ACCEPTED) {
            return true;
        }

        CHI::Eb::Xact::RNCacheStateMap<Config>& stateMap =
            rnStateMaps_[static_cast<std::uint32_t>(decodedRecord.nodeId)];
        const std::uint64_t time = static_cast<std::uint64_t>(
            std::max<qint64>(0, decodedRecord.timestamp));
        bool sampled = false;
        std::uint64_t address = 0;

        std::visit([&](const auto& flit) {
            using T = std::decay_t<decltype(flit)>;
            if constexpr (std::is_same_v<T, FlexibleReqFlit>) {
                if (decodedRecord.direction != FlitDirection::Tx) {
                    return;
                }
                const auto xactionFlit = makeEbXactionReqFlit<Config>(flit);
                address = normalizedCacheLineAddress(static_cast<std::uint64_t>(xactionFlit.Addr()));
                if (stateMap.NextTXREQ(address, time, xactionFlit) == CHI::Eb::Xact::XactDenial::ACCEPTED) {
                    sampled = true;
                }
            } else if constexpr (std::is_same_v<T, FlexibleRspFlit>) {
                if (!replayRecord.result.xaction) {
                    return;
                }
                address = normalizedCacheLineAddress(xactionFirstAddress(*replayRecord.result.xaction));
                const auto xactionFlit = makeEbXactionRspFlit<Config>(flit);
                CHI::Eb::Xact::XactDenialEnum denial = nullptr;
                if (decodedRecord.direction == FlitDirection::Rx
                    && replayRecord.result.xaction->GetFirst().IsREQ()) {
                    denial = stateMap.NextRXRSP(address, time, *replayRecord.result.xaction, xactionFlit);
                } else if (decodedRecord.direction == FlitDirection::Tx
                           && replayRecord.result.xaction->GetFirst().IsSNP()) {
                    denial = stateMap.NextTXRSP(address, time, *replayRecord.result.xaction, xactionFlit);
                }
                sampled = denial == CHI::Eb::Xact::XactDenial::ACCEPTED;
            } else if constexpr (std::is_same_v<T, FlexibleDatFlit>) {
                if (!replayRecord.result.xaction) {
                    return;
                }
                address = normalizedCacheLineAddress(xactionFirstAddress(*replayRecord.result.xaction));
                const auto xactionFlit = makeEbXactionDatFlit<Config>(flit);
                CHI::Eb::Xact::XactDenialEnum denial = nullptr;
                if (decodedRecord.direction == FlitDirection::Rx
                    && replayRecord.result.xaction->GetFirst().IsREQ()) {
                    denial = stateMap.NextRXDAT(address, time, *replayRecord.result.xaction, xactionFlit);
                } else if (decodedRecord.direction == FlitDirection::Tx
                           && replayRecord.result.xaction->GetFirst().IsSNP()) {
                    denial = stateMap.NextTXDAT(address, time, *replayRecord.result.xaction, xactionFlit);
                }
                sampled = denial == CHI::Eb::Xact::XactDenial::ACCEPTED;
            } else if constexpr (std::is_same_v<T, FlexibleSnpFlit>) {
                if (decodedRecord.direction != FlitDirection::Rx) {
                    return;
                }
                const auto xactionFlit = makeEbXactionSnpFlit<Config>(flit);
                address = normalizedCacheLineAddress(static_cast<std::uint64_t>(xactionFlit.Addr()) << 3U);
                if (stateMap.NextRXSNP(address, time, xactionFlit) == CHI::Eb::Xact::XactDenial::ACCEPTED) {
                    sampled = true;
                }
            }
        }, decodedRecord.flit);

        if (!sampled) {
            return true;
        }

        const CHI::Eb::Xact::CacheState state = stateMap.Get(address);
        return updateLine(static_cast<std::uint32_t>(decodedRecord.nodeId),
                          QString::fromStdString(CLog::NodeTypeToString(*nodeType)),
                          address,
                          decodedRecord.timestamp,
                          logicalRow,
                          state,
                          false,
                          error);
    }

    bool closeOpenLifetimes(const CLogBTraceMetadata& metadata, CLogBTraceLoadError& error)
    {
        std::vector<std::pair<CacheLineKey, ActiveCacheLineLifetime>> open;
        open.reserve(active_.size());
        for (const auto& entry : active_) {
            open.push_back(entry);
        }
        std::sort(open.begin(), open.end(), [](const auto& lhs, const auto& rhs) {
            if (lhs.first.rnNodeId != rhs.first.rnNodeId) {
                return lhs.first.rnNodeId < rhs.first.rnNodeId;
            }
            return lhs.first.lineAddress < rhs.first.lineAddress;
        });

        const std::uint64_t finalRow = metadata.totalRecords == 0 ? 0 : metadata.totalRecords - 1;
        for (const auto& [key, active] : open) {
            const auto typeFound = rnNodeTypes_.find(key.rnNodeId);
            CLogBTraceCacheLineLifetime lifetime;
            lifetime.rnNodeId = key.rnNodeId;
            lifetime.rnNodeType = typeFound == rnNodeTypes_.end() ? QStringLiteral("RN") : typeFound->second;
            lifetime.lineAddress = key.lineAddress;
            lifetime.startTimestamp = active.startTimestamp;
            lifetime.endTimestamp = metadata.lastTimestamp;
            lifetime.startLogicalRow = active.startLogicalRow;
            lifetime.endLogicalRow = finalRow;
            lifetime.startStateMask = active.startStateMask;
            lifetime.endStateMask = active.lastStateMask;
            lifetime.startStateText = active.startStateText;
            lifetime.endStateText = active.lastStateText;
            lifetime.openEnded = true;
            if (!emitLifetime(std::move(lifetime), error)) {
                return false;
            }
        }
        active_.clear();
        return true;
    }

private:
    bool updateLine(const std::uint32_t rnNodeId,
                    QString rnNodeType,
                    const std::uint64_t address,
                    const qint64 timestamp,
                    const std::uint64_t logicalRow,
                    const CHI::Eb::Xact::CacheState state,
                    const bool openEnded,
                    CLogBTraceLoadError& error)
    {
        Q_UNUSED(openEnded);
        const CacheLineKey key{rnNodeId, address};
        rnNodeTypes_.insert_or_assign(rnNodeId, std::move(rnNodeType));
        const bool alive = cacheStateAlive(state);
        const std::uint8_t stateMask = cacheStateMask(state);
        const QString stateText = cacheStateText(state);
        const auto found = active_.find(key);

        if (alive) {
            if (found == active_.end()) {
                active_.emplace(key, ActiveCacheLineLifetime{
                    .startTimestamp = timestamp,
                    .startLogicalRow = logicalRow,
                    .startStateMask = stateMask,
                    .startStateText = stateText,
                    .lastStateMask = stateMask,
                    .lastStateText = stateText,
                });
            } else {
                found->second.lastStateMask = stateMask;
                found->second.lastStateText = stateText;
            }
            return true;
        }

        if (found == active_.end()) {
            return true;
        }

        CLogBTraceCacheLineLifetime lifetime;
        lifetime.rnNodeId = key.rnNodeId;
        lifetime.rnNodeType = rnNodeTypes_[key.rnNodeId];
        lifetime.lineAddress = key.lineAddress;
        lifetime.startTimestamp = found->second.startTimestamp;
        lifetime.endTimestamp = timestamp;
        lifetime.startLogicalRow = found->second.startLogicalRow;
        lifetime.endLogicalRow = logicalRow;
        lifetime.startStateMask = found->second.startStateMask;
        lifetime.endStateMask = stateMask;
        lifetime.startStateText = found->second.startStateText;
        lifetime.endStateText = stateText;
        lifetime.openEnded = false;
        active_.erase(found);
        return emitLifetime(std::move(lifetime), error);
    }

    bool emitLifetime(CLogBTraceCacheLineLifetime lifetime, CLogBTraceLoadError& error) const
    {
        if (lifetime.endTimestamp < lifetime.startTimestamp) {
            lifetime.endTimestamp = lifetime.startTimestamp;
        }
        if (!callback_ || callback_(std::move(lifetime))) {
            return true;
        }
        setCancelledLoadError(error);
        return false;
    }

private:
    CLogBTraceCacheLineLifetimeCallback callback_;
    std::unordered_map<std::uint32_t, CHI::Eb::Xact::RNCacheStateMap<Config>> rnStateMaps_;
    std::unordered_map<std::uint32_t, QString> rnNodeTypes_;
    std::unordered_map<CacheLineKey, ActiveCacheLineLifetime, CacheLineKeyHash> active_;
};

template<typename Config>
bool buildEbCacheLineLifetimes(const QString& resolvedFilePath,
                               const CLogBTraceMetadata& metadata,
                               const EbMeasures& measures,
                               const CLogBTraceCacheLineLifetimeCallback& lifetimeCallback,
                               CLogBTraceLoadError& error,
                               const CLogBTraceLoadCallbacks& callbacks,
                               std::stop_token stopToken)
{
    const QString flitReport = buildFlitReport(metadata.parameters, measures);
    EbXactionIndexThreadState<Config> state;
    initializeEbXactionIndexState(state, metadata.nodeTypes);
    EbCacheLifetimeReplay<Config> lifetimeReplay(lifetimeCallback);

    const std::size_t totalRecords =
        static_cast<std::size_t>(std::min<std::uint64_t>(
            metadata.totalRecords,
            static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())));
    reportStage(callbacks.stage, CLogBTraceLoadStage::Decoding, 1, totalRecords);
    if (callbacks.stageProgress) {
        callbacks.stageProgress(0, totalRecords);
    }
    reportProgress(callbacks.progress, 0, 0);

    std::uint64_t processedRows = 0;
    std::uint64_t processedBytes = 0;
    std::uint64_t totalBytes = 0;
    for (const CLogBTraceBlockSummary& block : metadata.blocks) {
        totalBytes += block.serializedSize;
    }
    reportProgress(callbacks.progress, 0, totalBytes);

    for (std::size_t blockIndex = 0; blockIndex < metadata.blocks.size(); ++blockIndex) {
        if (stopToken.stop_requested()) {
            setCancelledLoadError(error);
            return false;
        }

        std::shared_ptr<CLog::CLogB::TagCHIRecords> block;
        if (!CLogBTraceLoader::loadBlockRecords(resolvedFilePath,
                                                metadata,
                                                blockIndex,
                                                block,
                                                error,
                                                callbacks,
                                                stopToken)) {
            return false;
        }
        if (!block) {
            continue;
        }

        std::vector<qulonglong> recordTimestamps;
        std::size_t failedRecordIndex = 0;
        if (!buildRecordTimestamps(*block, recordTimestamps, failedRecordIndex)) {
            setLoadError(error,
                         CLogBTraceLoadError::Type::Generic,
                         QStringLiteral("Failed to reconstruct timestamps for Cache lifetime replay."),
                         QStringLiteral("Timestamp deltas overflowed or were malformed in block %1 near local record %2.")
                             .arg(static_cast<qulonglong>(blockIndex))
                             .arg(static_cast<qulonglong>(failedRecordIndex)));
            return false;
        }

        const std::size_t recordCount = std::min(block->records.size(), recordTimestamps.size());
        for (std::size_t localIndex = 0; localIndex < recordCount; ++localIndex) {
            if (stopToken.stop_requested()) {
                setCancelledLoadError(error);
                return false;
            }

            EbCacheReplayRecord<Config> replayRecord;
            if (!processEbCacheReplaySourceRecord(metadata,
                                                  measures,
                                                  flitReport,
                                                  *block,
                                                  block->records[localIndex],
                                                  localIndex,
                                                  recordTimestamps[localIndex],
                                                  state,
                                                  replayRecord,
                                                  error)) {
                return false;
            }

            const std::uint64_t logicalRow =
                metadata.blocks[blockIndex].rowStart + static_cast<std::uint64_t>(localIndex);
            if (!lifetimeReplay.replay(metadata, replayRecord, logicalRow, error)) {
                return false;
            }

            ++processedRows;
            if ((processedRows % kXactionMergeProgressBatchSize) == 0U && callbacks.stageProgress) {
                callbacks.stageProgress(static_cast<std::size_t>(
                                            std::min<std::uint64_t>(processedRows, totalRecords)),
                                        totalRecords);
            }
        }

        processedBytes += metadata.blocks[blockIndex].serializedSize;
        reportProgress(callbacks.progress, processedBytes, totalBytes);
    }

    if (!lifetimeReplay.closeOpenLifetimes(metadata, error)) {
        return false;
    }
    if (callbacks.stageProgress) {
        callbacks.stageProgress(totalRecords, totalRecords);
    }
    reportProgress(callbacks.progress, totalBytes, totalBytes);
    reportStage(callbacks.stage, CLogBTraceLoadStage::Completed, 1, totalRecords);
    return true;
}

template<typename Config>
bool replayDecodedEbXactionIndexRecord(
    const CLogBTraceMetadata& metadata,
    const DecodedEbRecord& decodedRecord,
    EbXactionIndexThreadState<Config>& state,
    CLogBTraceXactionIndexRecord& indexRecord,
    bool& accepted)
{
    accepted = false;
    bool processed = false;

    std::visit([&](const auto& flit) {
        using T = std::decay_t<decltype(flit)>;
        if constexpr (std::is_same_v<T, FlexibleReqFlit>) {
            const auto xactionFlit = makeEbXactionReqFlit<Config>(flit);
            const EbXactionProcessResult<Config> result =
                processEbReqXactionByRecordNode(state.xactGlobal,
                                                state.xactGlobal.TOPOLOGY,
                                                state.rnJoint,
                                                state.snJoint,
                                                metadata.nodeTypes,
                                                decodedRecord,
                                                xactionFlit,
                                                state.nextXactionOrdinal);
            storeEbXactionIndexRecord(indexRecord, result);
            accepted = result.denial == CHI::Eb::Xact::XactDenial::ACCEPTED
                && result.ordinal.has_value();
            processed = true;
        } else if constexpr (std::is_same_v<T, FlexibleRspFlit>) {
            const auto xactionFlit = makeEbXactionRspFlit<Config>(flit);
            const EbXactionProcessResult<Config> result =
                processEbRspXactionByRecordNode(state.xactGlobal,
                                                state.xactGlobal.TOPOLOGY,
                                                state.rnJoint,
                                                state.snJoint,
                                                metadata.nodeTypes,
                                                decodedRecord,
                                                xactionFlit,
                                                state.nextXactionOrdinal);
            storeEbXactionIndexRecord(indexRecord, result);
            accepted = result.denial == CHI::Eb::Xact::XactDenial::ACCEPTED
                && result.ordinal.has_value();
            processed = true;
        } else if constexpr (std::is_same_v<T, FlexibleDatFlit>) {
            const auto xactionFlit = makeEbXactionDatFlit<Config>(flit);
            const EbXactionProcessResult<Config> result =
                processEbDatXactionByRecordNode(state.xactGlobal,
                                                state.xactGlobal.TOPOLOGY,
                                                state.rnJoint,
                                                state.snJoint,
                                                metadata.nodeTypes,
                                                decodedRecord,
                                                xactionFlit,
                                                state.nextXactionOrdinal);
            storeEbXactionIndexRecord(indexRecord, result);
            accepted = result.denial == CHI::Eb::Xact::XactDenial::ACCEPTED
                && result.ordinal.has_value();
            processed = true;
        } else if constexpr (std::is_same_v<T, FlexibleSnpFlit>) {
            const auto xactionFlit = makeEbXactionSnpFlit<Config>(flit);
            const EbXactionProcessResult<Config> result =
                processEbSnpXactionByRecordNode(state.xactGlobal,
                                                state.xactGlobal.TOPOLOGY,
                                                state.rnJoint,
                                                state.snJoint,
                                                metadata.nodeTypes,
                                                decodedRecord,
                                                xactionFlit,
                                                state.nextXactionOrdinal);
            storeEbXactionIndexRecord(indexRecord, result);
            accepted = result.denial == CHI::Eb::Xact::XactDenial::ACCEPTED
                && result.ordinal.has_value();
            processed = true;
        }
    }, decodedRecord.flit);

    if (!processed) {
        storeEbXactionNotIndexedRecord(indexRecord);
    }
    return true;
}

template<typename Config>
bool buildEbXactionIndexRecords(const QString& resolvedFilePath,
                                const CLogBTraceMetadata& metadata,
                                const EbMeasures& measures,
                                const CLogBTraceXactionIndexRecordCallback& recordCallback,
                                CLogBTraceLoadError& error,
                                const CLogBTraceLoadCallbacks& callbacks,
                                std::stop_token stopToken)
{
    const QString flitReport = buildFlitReport(metadata.parameters, measures);
    if (metadata.totalRecords
        > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("The trace is too large for in-memory Xaction merge indexing."));
        return false;
    }

    const std::size_t totalRecords = static_cast<std::size_t>(metadata.totalRecords);
    std::vector<PendingEbXactionIndexRecord> pendingRecords(totalRecords);
    std::vector<std::unique_ptr<EbXactionIndexSliceContext<Config>>> contexts;
    contexts.reserve(metadata.blocks.size() * kXactionThreadsPerRecordChunk);
    std::vector<std::size_t> blockContextCounts(metadata.blocks.size(), 0);

    for (std::size_t blockIndex = 0; blockIndex < metadata.blocks.size(); ++blockIndex) {
        const CLogBTraceBlockSummary& block = metadata.blocks[blockIndex];
        if (block.recordCount == 0) {
            continue;
        }

        const std::size_t sliceCount =
            std::min<std::size_t>(kXactionThreadsPerRecordChunk,
                                  static_cast<std::size_t>(block.recordCount));
        const std::size_t recordsPerSlice =
            (static_cast<std::size_t>(block.recordCount) + sliceCount - 1) / sliceCount;
        for (std::size_t sliceIndex = 0; sliceIndex < sliceCount; ++sliceIndex) {
            const std::size_t localBegin = sliceIndex * recordsPerSlice;
            if (localBegin >= block.recordCount) {
                break;
            }
            const std::size_t localEnd =
                std::min<std::size_t>(static_cast<std::size_t>(block.recordCount),
                                      localBegin + recordsPerSlice);
            auto context = std::make_unique<EbXactionIndexSliceContext<Config>>();
            context->contextIndex = contexts.size();
            context->blockIndex = blockIndex;
            context->localBegin = localBegin;
            context->localEnd = localEnd;
            context->logicalBegin = block.rowStart + localBegin;
            context->logicalEnd = block.rowStart + localEnd;
            initializeEbXactionIndexState(context->state, metadata.nodeTypes);
            contexts.push_back(std::move(context));
            ++blockContextCounts[blockIndex];
        }
    }

    std::vector<std::unique_ptr<EbXactionIndexBlockCacheEntry>> blockCache(metadata.blocks.size());
    for (std::size_t blockIndex = 0; blockIndex < blockContextCounts.size(); ++blockIndex) {
        if (blockContextCounts[blockIndex] == 0) {
            continue;
        }

        auto entry = std::make_unique<EbXactionIndexBlockCacheEntry>();
        entry->remainingContexts.store(blockContextCounts[blockIndex],
                                       std::memory_order_relaxed);
        blockCache[blockIndex] = std::move(entry);
    }

    const std::size_t workerCount = contexts.empty()
        ? 0
        : std::min<std::size_t>(kXactionGlobalThreadLimit, contexts.size());
    reportStage(callbacks.stage,
                CLogBTraceLoadStage::Decoding,
                workerCount,
                totalRecords);
    if (callbacks.stageProgress) {
        callbacks.stageProgress(0, totalRecords);
    }

    std::atomic<std::size_t> nextContextIndex = 0;
    std::atomic<std::size_t> indexedRecords = 0;
    std::atomic_bool failed = false;
    std::mutex errorMutex;
    std::mutex deniedMutex;
    std::mutex progressCallbackMutex;
    std::vector<DeniedEbXactionIndexFlit> deniedFlits;

    const auto publishStageProgress = [&](const std::size_t completed,
                                          const std::size_t total) {
        if (callbacks.stageProgress) {
            const std::lock_guard lock(progressCallbackMutex);
            callbacks.stageProgress(completed, total);
        }
    };
    const auto publishIndexProgress = [&]() {
        publishStageProgress(indexedRecords.load(std::memory_order_relaxed),
                             totalRecords);
    };

    const auto storeFailure = [&](CLogBTraceLoadError workerError) {
        bool expected = false;
        if (failed.compare_exchange_strong(expected,
                                           true,
                                           std::memory_order_acq_rel,
                                           std::memory_order_relaxed)) {
            const std::lock_guard lock(errorMutex);
            error = std::move(workerError);
        }
    };

    const auto loadCachedBlock =
        [&](const std::size_t blockIndex,
            std::shared_ptr<const CLog::CLogB::TagCHIRecords>& block,
            std::shared_ptr<const std::vector<qulonglong>>& recordTimestamps,
            CLogBTraceLoadError& workerError) -> bool {
        block.reset();
        recordTimestamps.reset();
        if (blockIndex >= blockCache.size() || !blockCache[blockIndex]) {
            setLoadError(workerError,
                         CLogBTraceLoadError::Type::Generic,
                         QStringLiteral("The Xaction index block cache does not match the trace metadata."));
            return false;
        }

        EbXactionIndexBlockCacheEntry& entry = *blockCache[blockIndex];
        {
            std::unique_lock lock(entry.mutex);
            while (entry.loading && !entry.ready && !entry.failed) {
                entry.readyCondition.wait(lock);
            }
            if (entry.ready) {
                block = entry.block;
                recordTimestamps = entry.recordTimestamps;
                return block && recordTimestamps;
            }
            if (entry.failed) {
                workerError = entry.error;
                return false;
            }
            entry.loading = true;
        }

        CLogBTraceLoadError loadError;
        std::shared_ptr<CLog::CLogB::TagCHIRecords> loadedBlock;
        bool ok = CLogBTraceLoader::loadBlockRecords(resolvedFilePath,
                                                     metadata,
                                                     blockIndex,
                                                     loadedBlock,
                                                     loadError,
                                                     {},
                                                     stopToken);
        auto loadedTimestamps = std::make_shared<std::vector<qulonglong>>();
        if (ok && !loadedBlock) {
            setLoadError(loadError,
                         CLogBTraceLoadError::Type::Generic,
                         QStringLiteral("The CHI record block loaded for Xaction indexing was empty."));
            ok = false;
        }
        if (ok) {
            std::size_t failedRecordIndex = 0;
            if (!buildRecordTimestamps(*loadedBlock, *loadedTimestamps, failedRecordIndex)) {
                setLoadError(loadError,
                             CLogBTraceLoadError::Type::Generic,
                             QStringLiteral("Record %1 timestamp exceeds the GUI timestamp range.")
                                 .arg(static_cast<qulonglong>(
                                     metadata.blocks[blockIndex].rowStart
                                     + failedRecordIndex + 1)));
                ok = false;
            }
        }

        {
            std::lock_guard lock(entry.mutex);
            entry.loading = false;
            if (ok) {
                entry.block = std::move(loadedBlock);
                entry.recordTimestamps = std::move(loadedTimestamps);
                entry.ready = true;
                block = entry.block;
                recordTimestamps = entry.recordTimestamps;
            } else {
                entry.error = loadError;
                entry.failed = true;
                workerError = std::move(loadError);
            }
        }
        entry.readyCondition.notify_all();
        return ok;
    };

    const auto releaseCachedBlockContext = [&](const std::size_t blockIndex) {
        if (blockIndex >= blockCache.size() || !blockCache[blockIndex]) {
            return;
        }

        EbXactionIndexBlockCacheEntry& entry = *blockCache[blockIndex];
        if (entry.remainingContexts.fetch_sub(1, std::memory_order_acq_rel) != 1) {
            return;
        }

        const std::lock_guard lock(entry.mutex);
        entry.block.reset();
        entry.recordTimestamps.reset();
    };

    std::vector<std::jthread> workers;
    workers.reserve(workerCount);
    for (std::size_t workerIndex = 0; workerIndex < workerCount; ++workerIndex) {
        workers.emplace_back([&]() {
            try {
                for (;;) {
                    if (stopToken.stop_requested()
                        || failed.load(std::memory_order_acquire)) {
                        return;
                    }

                    const std::size_t contextIndex =
                        nextContextIndex.fetch_add(1, std::memory_order_relaxed);
                    if (contextIndex >= contexts.size()) {
                        return;
                    }

                    EbXactionIndexSliceContext<Config>& context = *contexts[contextIndex];
                    CLogBTraceLoadError workerError;
                    std::shared_ptr<const CLog::CLogB::TagCHIRecords> block;
                    std::shared_ptr<const std::vector<qulonglong>> recordTimestamps;
                    if (!loadCachedBlock(context.blockIndex,
                                         block,
                                         recordTimestamps,
                                         workerError)) {
                        releaseCachedBlockContext(context.blockIndex);
                        storeFailure(std::move(workerError));
                        return;
                    }
                    if (!block || !recordTimestamps) {
                        setLoadError(workerError,
                                     CLogBTraceLoadError::Type::Generic,
                                     QStringLiteral("The CHI record block loaded for Xaction indexing was empty."));
                        releaseCachedBlockContext(context.blockIndex);
                        storeFailure(std::move(workerError));
                        return;
                    }

                    if (context.localEnd > block->records.size()
                        || context.localEnd > recordTimestamps->size()) {
                        setLoadError(workerError,
                                     CLogBTraceLoadError::Type::Generic,
                                     QStringLiteral("The Xaction index slice exceeds the loaded CHI record block."));
                        releaseCachedBlockContext(context.blockIndex);
                        storeFailure(std::move(workerError));
                        return;
                    }

                    std::vector<DeniedEbXactionIndexFlit> localDeniedFlits;
                    for (std::size_t localIndex = context.localBegin;
                         localIndex < context.localEnd;
                         ++localIndex) {
                        if (stopToken.stop_requested()
                            || failed.load(std::memory_order_acquire)) {
                            releaseCachedBlockContext(context.blockIndex);
                            return;
                        }

                        const CLogBTraceBlockSummary& blockSummary =
                            metadata.blocks[context.blockIndex];
                        const std::uint64_t logicalRow = blockSummary.rowStart + localIndex;
                        if (logicalRow >= metadata.totalRecords) {
                            setLoadError(workerError,
                                         CLogBTraceLoadError::Type::Generic,
                                         QStringLiteral("The Xaction index row map does not match the trace metadata."));
                            releaseCachedBlockContext(context.blockIndex);
                            storeFailure(std::move(workerError));
                            return;
                        }

                        CLogBTraceXactionIndexRecord indexRecord;
                        DecodedEbRecord decodedRecord;
                        bool deniedByJoint = false;
                        if (!processEbXactionIndexSourceRecord<Config>(metadata,
                                                                       measures,
                                                                       flitReport,
                                                                       *block,
                                                                       block->records[localIndex],
                                                                       localIndex,
                                                                       (*recordTimestamps)[localIndex],
                                                                       context.state,
                                                                       indexRecord,
                                                                       &decodedRecord,
                                                                       &deniedByJoint,
                                                                       workerError)) {
                            releaseCachedBlockContext(context.blockIndex);
                            storeFailure(std::move(workerError));
                            return;
                        }

                        PendingEbXactionIndexRecord& pending =
                            pendingRecords[static_cast<std::size_t>(logicalRow)];
                        pending.record = std::move(indexRecord);
                        setPendingEbXactionRecordOwner(pending, context.contextIndex);
                        if (deniedByJoint) {
                            localDeniedFlits.push_back(DeniedEbXactionIndexFlit{
                                .logicalRow = logicalRow,
                                .sourceContext = context.contextIndex,
                                .decodedRecord = std::move(decodedRecord),
                            });
                        }

                        const std::size_t completed =
                            indexedRecords.fetch_add(1, std::memory_order_relaxed) + 1;
                        if (callbacks.stageProgress
                            && ((completed % kXactionMergeProgressBatchSize) == 0
                                || completed == totalRecords)) {
                            publishStageProgress(completed, totalRecords);
                        }
                    }

                    if (!localDeniedFlits.empty()) {
                        const std::lock_guard lock(deniedMutex);
                        deniedFlits.insert(deniedFlits.end(),
                                           std::make_move_iterator(localDeniedFlits.begin()),
                                           std::make_move_iterator(localDeniedFlits.end()));
                    }
                    releaseCachedBlockContext(context.blockIndex);
                }
            } catch (const std::bad_alloc&) {
                CLogBTraceLoadError workerError;
                setLoadError(workerError,
                             CLogBTraceLoadError::Type::Generic,
                             QStringLiteral("Out of memory while building the Xaction index."));
                storeFailure(std::move(workerError));
            } catch (...) {
                CLogBTraceLoadError workerError;
                setLoadError(workerError,
                             CLogBTraceLoadError::Type::Generic,
                             QStringLiteral("Unexpected failure while building the Xaction index."));
                storeFailure(std::move(workerError));
            }
        });
    }
    workers.clear();

    if (stopToken.stop_requested()) {
        setCancelledLoadError(error);
        return false;
    }
    if (failed.load(std::memory_order_acquire)) {
        return false;
    }

    publishIndexProgress();
    std::sort(deniedFlits.begin(),
              deniedFlits.end(),
              [](const DeniedEbXactionIndexFlit& lhs,
                 const DeniedEbXactionIndexFlit& rhs) {
                  return lhs.logicalRow < rhs.logicalRow;
              });

    reportStage(callbacks.stage,
                CLogBTraceLoadStage::Formatting,
                0,
                deniedFlits.size());
    std::size_t mergeWorkTotal = 0;
    for (const std::unique_ptr<EbXactionIndexSliceContext<Config>>& context : contexts) {
        if (!context->state.rnJoint.HasInflight()
            && !context->state.snJoint.HasInflight()) {
            continue;
        }
        mergeWorkTotal += static_cast<std::size_t>(std::count_if(
            deniedFlits.begin(),
            deniedFlits.end(),
            [&](const DeniedEbXactionIndexFlit& deniedFlit) {
                return deniedFlit.sourceContext != context->contextIndex
                    && deniedFlit.logicalRow >= context->logicalEnd;
            }));
    }
    std::size_t mergeWorkCompleted = 0;
    if (callbacks.stageProgress) {
        publishStageProgress(0, mergeWorkTotal);
    }
    for (const std::unique_ptr<EbXactionIndexSliceContext<Config>>& context : contexts) {
        if (stopToken.stop_requested()) {
            setCancelledLoadError(error);
            return false;
        }

        for (const DeniedEbXactionIndexFlit& deniedFlit : deniedFlits) {
            if (stopToken.stop_requested()) {
                setCancelledLoadError(error);
                return false;
            }
            if (!context->state.rnJoint.HasInflight()
                && !context->state.snJoint.HasInflight()) {
                break;
            }
            if (deniedFlit.sourceContext == context->contextIndex
                || deniedFlit.logicalRow < context->logicalEnd) {
                continue;
            }

            EbXactionIndexThreadState<Config> trialState = context->state;
            CLogBTraceXactionIndexRecord mergedRecord;
            bool accepted = false;
            replayDecodedEbXactionIndexRecord<Config>(metadata,
                                                      deniedFlit.decodedRecord,
                                                      trialState,
                                                      mergedRecord,
                                                      accepted);
            if (!accepted) {
                ++mergeWorkCompleted;
                if (callbacks.stageProgress
                    && ((mergeWorkCompleted % kXactionMergeProgressBatchSize) == 0
                        || mergeWorkCompleted == mergeWorkTotal)) {
                    publishStageProgress(mergeWorkCompleted, mergeWorkTotal);
                }
                continue;
            }

            context->state = std::move(trialState);
            PendingEbXactionIndexRecord& pending =
                pendingRecords[static_cast<std::size_t>(deniedFlit.logicalRow)];
            if (!pending.record.indexed) {
                pending.record = std::move(mergedRecord);
                setPendingEbXactionRecordOwner(pending, context->contextIndex);
            }
            ++mergeWorkCompleted;
            if (callbacks.stageProgress
                && ((mergeWorkCompleted % kXactionMergeProgressBatchSize) == 0
                    || mergeWorkCompleted == mergeWorkTotal)) {
                publishStageProgress(mergeWorkCompleted, mergeWorkTotal);
            }
        }
    }
    if (callbacks.stageProgress) {
        publishStageProgress(mergeWorkTotal, mergeWorkTotal);
    }

    std::map<std::pair<std::size_t, std::uint64_t>, std::uint64_t> globalOrdinals;
    std::uint64_t nextGlobalOrdinal = 1;
    for (PendingEbXactionIndexRecord& pending : pendingRecords) {
        if (!pending.record.indexed
            || pending.ownerContext == kNoXactionContext
            || pending.ownerOrdinal == 0) {
            continue;
        }

        const std::pair<std::size_t, std::uint64_t> key{
            pending.ownerContext,
            pending.ownerOrdinal,
        };
        auto [it, inserted] = globalOrdinals.emplace(key, nextGlobalOrdinal);
        if (inserted) {
            ++nextGlobalOrdinal;
        }
        pending.record.xactionOrdinal = it->second;
        pending.record.transactionGroupKey =
            QStringLiteral("xaction|%1")
                .arg(static_cast<qulonglong>(pending.record.xactionOrdinal));
    }

    reportStage(callbacks.stage,
                CLogBTraceLoadStage::Formatting,
                1,
                totalRecords);
    if (callbacks.stageProgress) {
        publishStageProgress(0, totalRecords);
    }
    for (std::size_t row = 0; row < pendingRecords.size(); ++row) {
        if (stopToken.stop_requested()) {
            setCancelledLoadError(error);
            return false;
        }
        if (recordCallback
            && !recordCallback(static_cast<std::uint64_t>(row),
                               std::move(pendingRecords[row].record))) {
            setLoadError(error,
                         CLogBTraceLoadError::Type::Generic,
                         QStringLiteral("Could not write Xaction index data."));
            return false;
        }
        if (callbacks.stageProgress
            && (((row + 1) % kXactionMergeProgressBatchSize) == 0
                || (row + 1) == pendingRecords.size())) {
            publishStageProgress(row + 1, pendingRecords.size());
        }
    }

    return true;
}

bool decodeTagSlice(const CLogBTraceMetadata& metadata,
                    const std::size_t blockIndex,
                    const CLog::CLogB::TagCHIRecords& fullTag,
                    const std::size_t localBegin,
                    const std::size_t rowCount,
                    std::vector<FlitRecord>& records,
                    CLogBTraceLoadError& error,
                    const CLogBTraceLoadCallbacks& callbacks,
                    std::stop_token stopToken,
                    const bool allowParallelDecode = true,
                    const bool trackXactions = true)
{
    if (blockIndex >= metadata.blocks.size()) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("Requested block index %1 is out of range.")
                         .arg(static_cast<qulonglong>(blockIndex)));
        return false;
    }

    if (localBegin > fullTag.records.size()
        || rowCount > (fullTag.records.size() - localBegin)) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("Requested row slice exceeds the decoded CHI record block."));
        return false;
    }

    EbMeasures ebMeasures;
    BMeasures bMeasures;
    switch (metadata.parameters.GetIssue()) {
    case CLog::Issue::Eb:
        if (!prepareEbTraceDecoding(metadata.parameters, ebMeasures, error)) {
            return false;
        }
        break;
    case CLog::Issue::B:
        if (!prepareBTraceDecoding(metadata.parameters, bMeasures, error)) {
            return false;
        }
        break;
    default:
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("This CLog.B file uses unsupported CHI Issue %1.")
                         .arg(issueName(metadata.parameters.GetIssue())),
                     QStringLiteral("CloganGUI currently decodes CHI Issue B and CHI Issue E.b traces."),
                     describeParametersBlock(metadata.parameters));
        return false;
    }

    CLog::CLogB::TagCHIRecords selectedTag;
    selectedTag.head = fullTag.head;
    std::size_t failedRecordIndex = 0;
    qulonglong accumulatedBase = 0;
    if (!accumulateTagTimestampBase(fullTag, localBegin, accumulatedBase, failedRecordIndex)) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("Record %1 timestamp exceeds the GUI timestamp range.")
                         .arg(static_cast<qulonglong>(failedRecordIndex + 1)));
        return false;
    }
    selectedTag.head.timeBase = static_cast<std::uint64_t>(accumulatedBase);
    selectedTag.records.assign(fullTag.records.begin() + static_cast<std::ptrdiff_t>(localBegin),
                               fullTag.records.begin() + static_cast<std::ptrdiff_t>(localBegin + rowCount));

    const CLogBTraceBlockSummary& block = metadata.blocks[blockIndex];
    switch (metadata.parameters.GetIssue()) {
    case CLog::Issue::Eb:
        return appendDecodedTag(metadata.parameters,
                                ebMeasures,
                                selectedTag,
                                metadata.nodeAnnotations,
                                metadata.nodeTypes,
                                records,
                                error,
                                callbacks,
                                0,
                                block.serializedSize,
                                block.serializedSize,
                                stopToken,
                                allowParallelDecode,
                                trackXactions);
    case CLog::Issue::B:
        return appendDecodedBTag(metadata.parameters,
                                 bMeasures,
                                 selectedTag,
                                 metadata.nodeAnnotations,
                                 records,
                                 error,
                                 callbacks,
                                 0,
                                 block.serializedSize,
                                 block.serializedSize,
                                 stopToken,
                                 allowParallelDecode);
    default:
        return false;
    }
}

bool decodeSingleRawRecord(const CLogBTraceMetadata& metadata,
                           const qint64 timestamp,
                           const CLog::Channel channel,
                           const std::uint16_t nodeId,
                           const std::uint8_t flitLength,
                           const std::vector<std::uint32_t>& flitWords,
                           FlitRecord& record,
                           CLogBTraceLoadError& error)
{
    error = {};
    const std::size_t wordCount = (static_cast<std::size_t>(flitLength) + 3U) / 4U;
    if (flitLength == 0 || flitWords.size() < wordCount) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("The edited flit has no raw payload to decode."));
        return false;
    }

    CLog::CLogB::TagCHIRecords tag;
    tag.head.timeBase = static_cast<std::uint64_t>(timestamp);
    tag.head.compressed = 0;
    tag.records.resize(1);
    CLog::CLogB::TagCHIRecords::Record& source = tag.records.front();
    source.timeShift = 0;
    source.nodeId = nodeId;
    source.channel = channel;
    source.flitLength = flitLength;
    source.flit = std::shared_ptr<std::uint32_t[]>(new std::uint32_t[wordCount]());
    std::copy_n(flitWords.data(), wordCount, source.flit.get());

    std::vector<FlitRecord> rows;
    if (!decodeTagSlice(metadata,
                        0,
                        tag,
                        0,
                        1,
                        rows,
                        error,
                        {},
                        {},
                        false,
                        false)
        || rows.empty()) {
        return false;
    }

    record = std::move(rows.front());
    return true;
}

}  // namespace

bool CLogBTraceLoader::scanFile(const QString& filePath,
                                CLogBTraceMetadata& metadata,
                                CLogBTraceLoadError& error,
                                const CLogBTraceLoadCallbacks& callbacks,
                                std::stop_token stopToken)
{
    metadata = {};
    metadata.filePath = filePath;
    error = {};

    reportStage(callbacks.stage, CLogBTraceLoadStage::Opening, 0, 0);

    const std::filesystem::path path(filePath.toStdWString());
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("Unable to open %1.")
                         .arg(QFileInfo(filePath).fileName()));
        return false;
    }

    std::vector<char> streamBuffer(4U << 20);
    input.rdbuf()->pubsetbuf(streamBuffer.data(), static_cast<std::streamsize>(streamBuffer.size()));

    std::error_code fileSizeError;
    const std::uint64_t totalBytes = std::filesystem::file_size(path, fileSizeError);
    reportProgress(callbacks.progress, 0, fileSizeError ? 0U : totalBytes);
    reportStage(callbacks.stage, CLogBTraceLoadStage::Parsing, 0, 0);

    bool hasParameters = false;
    std::uint64_t processedBytes = 0;
    while (true) {
        if (stopToken.stop_requested()) {
            setCancelledLoadError(error);
            return false;
        }

        RawTagHeader header;
        std::string readError;
        if (!readRawTagHeader(input, header, readError, processedBytes)) {
            if (stopToken.stop_requested()) {
                setCancelledLoadError(error);
                return false;
            }

            setLoadError(error,
                         CLogBTraceLoadError::Type::Generic,
                         QStringLiteral("CLog.B metadata scan failed."),
                         QStringLiteral("The file could not be scanned as a valid CLog.B stream."),
                         QString::fromStdString(readError));
            return false;
        }

        if (header.type == CLog::CLogB::Encodings::_EOF) {
            reportProgress(callbacks.progress,
                           fileSizeError ? processedBytes : totalBytes,
                           fileSizeError ? 0U : totalBytes);
            break;
        }

        switch (header.type) {
        case CLog::CLogB::Encodings::CHI_PARAMETERS: {
            CLog::CLogB::TagCHIParameters tag;
            if (!tag.Deserialize(input, readError) || !advanceToTagEnd(input, header, readError)) {
                if (stopToken.stop_requested()) {
                    setCancelledLoadError(error);
                    return false;
                }

                setLoadError(error,
                             CLogBTraceLoadError::Type::Generic,
                             QStringLiteral("CLog.B metadata scan failed."),
                             QStringLiteral("The CHI parameter block could not be parsed."),
                             QString::fromStdString(readError));
                return false;
            }

            metadata.parameters = tag.parameters;
            hasParameters = true;
            break;
        }

        case CLog::CLogB::Encodings::CHI_TOPOS: {
            CLog::CLogB::TagCHITopologies tag;
            if (!tag.Deserialize(input, readError) || !advanceToTagEnd(input, header, readError)) {
                if (stopToken.stop_requested()) {
                    setCancelledLoadError(error);
                    return false;
                }

                setLoadError(error,
                             CLogBTraceLoadError::Type::Generic,
                             QStringLiteral("CLog.B metadata scan failed."),
                             QStringLiteral("The CHI topology block could not be parsed."),
                             QString::fromStdString(readError));
                return false;
            }

            for (const auto& node : tag.nodes) {
                metadata.nodeAnnotations.insert_or_assign(
                    node.id,
                    InternDisplayString(
                        QStringLiteral("Captured at node %1 (%2).")
                            .arg(node.id)
                            .arg(QString::fromStdString(CLog::NodeTypeToString(node.type)))));
                metadata.nodeTypes.insert_or_assign(node.id, node.type);
            }
            break;
        }

        case CLog::CLogB::Encodings::CHI_RECORDS: {
            if (!hasParameters) {
                setLoadError(error,
                             CLogBTraceLoadError::Type::Generic,
                             QStringLiteral("CLog.B file is missing CHI parameters."),
                             QStringLiteral("The first record block appeared before the file declared its CHI configuration."));
                return false;
            }

            CLogBTraceBlockSummary block;
            block.fileOffset = header.fileOffset;
            block.serializedSize = kCLogBTagHeaderBytes + header.payloadLength;
            block.rowStart = metadata.totalRecords;
            if (!scanRecordBlock(input,
                                 header,
                                 block,
                                 readError,
                                 callbacks,
                                 fileSizeError ? 0U : totalBytes,
                                 stopToken)) {
                if (stopToken.stop_requested()) {
                    setCancelledLoadError(error);
                    return false;
                }

                setLoadError(error,
                             CLogBTraceLoadError::Type::Generic,
                             QStringLiteral("CLog.B metadata scan failed."),
                             QStringLiteral("A CHI record block could not be scanned."),
                             QString::fromStdString(readError));
                return false;
            }

            metadata.totalRecords += block.recordCount;
            if (block.recordCount > 0) {
                if (metadata.blocks.empty() || metadata.totalRecords == block.recordCount) {
                    metadata.firstTimestamp = block.firstTimestamp;
                }
                metadata.lastTimestamp = block.lastTimestamp;
            }
            for (std::size_t channelIndex = 0; channelIndex < metadata.channelCounts.size(); ++channelIndex) {
                metadata.channelCounts[channelIndex] += block.channelCounts[channelIndex];
                mergeFlitLengthMask(metadata.flitLengthMasks[channelIndex],
                                    block.flitLengthMasks[channelIndex]);
            }
            metadata.blocks.push_back(block);
            break;
        }

        default:
            setLoadError(error,
                         CLogBTraceLoadError::Type::Generic,
                         QStringLiteral("Unsupported CLog.B tag type 0x%1.")
                            .arg(QString::number(static_cast<int>(header.type), 16)
                                     .toUpper()
                                     .rightJustified(2, QLatin1Char('0'))));
            return false;
        }

        processedBytes = streamBytePosition(input, processedBytes);
        reportProgress(callbacks.progress, processedBytes, fileSizeError ? 0U : totalBytes);
    }

    if (!hasParameters) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("CLog.B file does not contain a CHI parameters tag."));
        return false;
    }

    reportProgress(callbacks.progress,
                   fileSizeError ? processedBytes : totalBytes,
                   fileSizeError ? 0U : totalBytes);
    reportStage(callbacks.stage, CLogBTraceLoadStage::Completed, 0, 0);
    return true;
}

bool CLogBTraceLoader::loadBlockRecords(const QString& filePath,
                                        const CLogBTraceMetadata& metadata,
                                        const std::size_t blockIndex,
                                        std::shared_ptr<CLog::CLogB::TagCHIRecords>& block,
                                        CLogBTraceLoadError& error,
                                        const CLogBTraceLoadCallbacks& callbacks,
                                        std::stop_token stopToken)
{
    block.reset();
    error = {};

    if (blockIndex >= metadata.blocks.size()) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("Requested block index %1 is out of range.")
                         .arg(static_cast<qulonglong>(blockIndex)));
        return false;
    }

    if (stopToken.stop_requested()) {
        setCancelledLoadError(error);
        return false;
    }

    const QString resolvedFilePath = resolveTraceFilePath(filePath, metadata);
    if (resolvedFilePath.isEmpty()) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("No trace file path was provided for block loading."));
        return false;
    }

    reportStage(callbacks.stage, CLogBTraceLoadStage::Opening, 0, 0);

    const std::filesystem::path path(resolvedFilePath.toStdWString());
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("Unable to open %1.")
                         .arg(QFileInfo(resolvedFilePath).fileName()));
        return false;
    }

    std::vector<char> streamBuffer(4U << 20);
    input.rdbuf()->pubsetbuf(streamBuffer.data(), static_cast<std::streamsize>(streamBuffer.size()));
    reportStage(callbacks.stage, CLogBTraceLoadStage::Parsing, 0, 0);

    const CLogBTraceBlockSummary& blockSummary = metadata.blocks[blockIndex];
    std::string ioError;
    if (!seekStreamAbsolute(input, blockSummary.fileOffset, ioError)) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("Unable to seek within %1.")
                         .arg(QFileInfo(resolvedFilePath).fileName()),
                     QString(),
                     QString::fromStdString(ioError));
        return false;
    }

    CLog::CLogB::Reader reader;
    std::shared_ptr<CLog::CLogB::Tag> tag = reader.Next(input, ioError);
    if (!tag) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("CLog.B parsing failed."),
                     QStringLiteral("The requested record block could not be decoded."),
                     QString::fromStdString(ioError));
        return false;
    }

    if (tag->type != CLog::CLogB::Encodings::CHI_RECORDS) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("Expected a CHI records block at file offset %1.")
                         .arg(blockSummary.fileOffset));
        return false;
    }

    block = std::static_pointer_cast<CLog::CLogB::TagCHIRecords>(tag);
    if (block->records.size() != blockSummary.recordCount) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("The trace file changed after its metadata was scanned."));
        block.reset();
        return false;
    }

    reportProgress(callbacks.progress, blockSummary.serializedSize, blockSummary.serializedSize);
    reportStage(callbacks.stage, CLogBTraceLoadStage::Completed, 0, 0);
    return true;
}

bool CLogBTraceLoader::decodeBlockRows(const CLogBTraceMetadata& metadata,
                                       const std::size_t blockIndex,
                                       const CLog::CLogB::TagCHIRecords& block,
                                       const std::size_t localBegin,
                                       const std::size_t rowCount,
                                       std::vector<FlitRecord>& records,
                                       CLogBTraceLoadError& error,
                                       const CLogBTraceLoadCallbacks& callbacks,
                                       std::stop_token stopToken,
                                       const bool allowParallelDecode)
{
    records.clear();
    error = {};

    if (stopToken.stop_requested()) {
        setCancelledLoadError(error);
        return false;
    }

    reportStage(callbacks.stage, CLogBTraceLoadStage::Decoding, 0, rowCount);
    const bool trackXactions = false;
    const bool ok = decodeTagSlice(metadata,
                                   blockIndex,
                                   block,
                                   localBegin,
                                   rowCount,
                                   records,
                                   error,
                                   callbacks,
                                   stopToken,
                                   allowParallelDecode,
                                   trackXactions);
    if (!ok) {
        return false;
    }

    reportStage(callbacks.stage, CLogBTraceLoadStage::Completed, 0, 0);
    return true;
}

bool CLogBTraceLoader::decodeRawRecord(const CLogBTraceMetadata& metadata,
                                       const qint64 timestamp,
                                       const CLog::Channel channel,
                                       const std::uint16_t nodeId,
                                       const std::uint8_t flitLength,
                                       const std::vector<std::uint32_t>& flitWords,
                                       FlitRecord& record,
                                       CLogBTraceLoadError& error)
{
    return decodeSingleRawRecord(metadata,
                                 timestamp,
                                 channel,
                                 nodeId,
                                 flitLength,
                                 flitWords,
                                 record,
                                 error);
}

bool CLogBTraceLoader::collectBlockRowsMatchingFilter(const CLogBTraceMetadata& metadata,
                                                      const std::size_t blockIndex,
                                                      const CLog::CLogB::TagCHIRecords& block,
                                                      const CLogBTraceFastFilter& filter,
                                                      std::vector<std::size_t>& localRows,
                                                      CLogBTraceLoadError& error,
                                                      std::stop_token stopToken)
{
    localRows.clear();
    error = {};

    if (blockIndex >= metadata.blocks.size()) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("Requested block index %1 is out of range.")
                         .arg(static_cast<qulonglong>(blockIndex)));
        return false;
    }

    EbMeasures ebMeasures;
    BMeasures bMeasures;
    switch (metadata.parameters.GetIssue()) {
    case CLog::Issue::Eb:
        if (!prepareEbTraceDecoding(metadata.parameters, ebMeasures, error)) {
            return false;
        }
        break;
    case CLog::Issue::B:
        if (!prepareBTraceDecoding(metadata.parameters, bMeasures, error)) {
            return false;
        }
        break;
    default:
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("This CLog.B file uses unsupported CHI Issue %1.")
                         .arg(issueName(metadata.parameters.GetIssue())),
                     QStringLiteral("CloganGUI currently decodes CHI Issue B and CHI Issue E.b traces."),
                     describeParametersBlock(metadata.parameters));
        return false;
    }

    localRows.reserve(block.records.size());
    for (std::size_t localIndex = 0; localIndex < block.records.size(); ++localIndex) {
        if (stopToken.stop_requested()) {
            setCancelledLoadError(error);
            return false;
        }

        bool matched = false;
        switch (metadata.parameters.GetIssue()) {
        case CLog::Issue::Eb:
            matched = recordMatchesRawFastFilter(filter,
                                                 metadata.parameters,
                                                 ebMeasures,
                                                 block.records[localIndex]);
            break;
        case CLog::Issue::B:
            matched = recordMatchesRawFastFilter(filter,
                                                 metadata.parameters,
                                                 bMeasures,
                                                 block.records[localIndex]);
            break;
        default:
            return false;
        }

        if (matched) {
            localRows.push_back(localIndex);
        }
    }

    return true;
}

bool CLogBTraceLoader::collectBlockRowsMatchingFilterAndBuildRecords(const CLogBTraceMetadata& metadata,
                                                                     const std::size_t blockIndex,
                                                                     const CLog::CLogB::TagCHIRecords& block,
                                                                     const CLogBTraceFastFilter& filter,
                                                                     std::vector<std::size_t>& localRows,
                                                                     std::vector<CLogBTraceFastRecordInfo>& fastRecords,
                                                                     CLogBTraceLoadError& error,
                                                                     std::stop_token stopToken)
{
    localRows.clear();
    fastRecords.clear();
    error = {};

    if (blockIndex >= metadata.blocks.size()) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("Requested block index %1 is out of range.")
                         .arg(static_cast<qulonglong>(blockIndex)));
        return false;
    }

    EbMeasures ebMeasures;
    BMeasures bMeasures;
    switch (metadata.parameters.GetIssue()) {
    case CLog::Issue::Eb:
        if (!prepareEbTraceDecoding(metadata.parameters, ebMeasures, error)) {
            return false;
        }
        break;
    case CLog::Issue::B:
        if (!prepareBTraceDecoding(metadata.parameters, bMeasures, error)) {
            return false;
        }
        break;
    default:
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("This CLog.B file uses unsupported CHI Issue %1.")
                         .arg(issueName(metadata.parameters.GetIssue())),
                     QStringLiteral("CloganGUI currently decodes CHI Issue B and CHI Issue E.b traces."),
                     describeParametersBlock(metadata.parameters));
        return false;
    }

    localRows.reserve(block.records.size());
    fastRecords.reserve(block.records.size());
    std::vector<qulonglong> recordTimestamps;
    std::size_t failedRecordIndex = 0;
    if (!buildRecordTimestamps(block, recordTimestamps, failedRecordIndex)) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("Record %1 timestamp exceeds the GUI timestamp range.")
                         .arg(static_cast<qulonglong>(failedRecordIndex + 1)));
        return false;
    }
    for (std::size_t localIndex = 0; localIndex < block.records.size(); ++localIndex) {
        if (stopToken.stop_requested()) {
            setCancelledLoadError(error);
            return false;
        }

        CLogBTraceFastRecordInfo info;
        bool ok = false;
        switch (metadata.parameters.GetIssue()) {
        case CLog::Issue::Eb:
            ok = extractFastRecordInfo(metadata.parameters, ebMeasures, block.records[localIndex], info);
            break;
        case CLog::Issue::B:
            ok = extractFastRecordInfo(metadata.parameters, bMeasures, block.records[localIndex], info);
            break;
        default:
            return false;
        }

        if (!ok) {
            setLoadError(error,
                         CLogBTraceLoadError::Type::Generic,
                         QStringLiteral("Unable to extract fast-filter fields from record %1 in block %2.")
                             .arg(static_cast<qulonglong>(localIndex + 1))
                             .arg(static_cast<qulonglong>(blockIndex)));
            localRows.clear();
            fastRecords.clear();
            return false;
        }

        info.timestamp = static_cast<qint64>(recordTimestamps[localIndex]);
        if (fastRecordMatchesFilter(metadata.parameters, filter, info)) {
            localRows.push_back(localIndex);
        }
        fastRecords.push_back(std::move(info));
    }

    return true;
}

QString CLogBTraceLoader::opcodeDisplayValue(const CLog::Parameters& params,
                                             const CLogBTraceFastRecordInfo& info)
{
    return fastOpcodeDisplayValue(params, info);
}

bool CLogBTraceLoader::buildBlockFastRecords(const CLogBTraceMetadata& metadata,
                                             const std::size_t blockIndex,
                                             const CLog::CLogB::TagCHIRecords& block,
                                             std::vector<CLogBTraceFastRecordInfo>& records,
                                             CLogBTraceLoadError& error,
                                             std::stop_token stopToken)
{
    records.clear();
    error = {};

    if (blockIndex >= metadata.blocks.size()) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("Requested block index %1 is out of range.")
                         .arg(static_cast<qulonglong>(blockIndex)));
        return false;
    }

    EbMeasures ebMeasures;
    BMeasures bMeasures;
    switch (metadata.parameters.GetIssue()) {
    case CLog::Issue::Eb:
        if (!prepareEbTraceDecoding(metadata.parameters, ebMeasures, error)) {
            return false;
        }
        break;
    case CLog::Issue::B:
        if (!prepareBTraceDecoding(metadata.parameters, bMeasures, error)) {
            return false;
        }
        break;
    default:
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("This CLog.B file uses unsupported CHI Issue %1.")
                         .arg(issueName(metadata.parameters.GetIssue())),
                     QStringLiteral("CloganGUI currently decodes CHI Issue B and CHI Issue E.b traces."),
                     describeParametersBlock(metadata.parameters));
        return false;
    }

    records.reserve(block.records.size());
    std::vector<qulonglong> recordTimestamps;
    std::size_t failedRecordIndex = 0;
    if (!buildRecordTimestamps(block, recordTimestamps, failedRecordIndex)) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("Record %1 timestamp exceeds the GUI timestamp range.")
                         .arg(static_cast<qulonglong>(failedRecordIndex + 1)));
        return false;
    }
    for (std::size_t localIndex = 0; localIndex < block.records.size(); ++localIndex) {
        if (stopToken.stop_requested()) {
            setCancelledLoadError(error);
            return false;
        }

        CLogBTraceFastRecordInfo info;
        bool ok = false;
        switch (metadata.parameters.GetIssue()) {
        case CLog::Issue::Eb:
            ok = extractFastRecordInfo(metadata.parameters, ebMeasures, block.records[localIndex], info);
            break;
        case CLog::Issue::B:
            ok = extractFastRecordInfo(metadata.parameters, bMeasures, block.records[localIndex], info);
            break;
        default:
            return false;
        }

        if (!ok) {
            setLoadError(error,
                         CLogBTraceLoadError::Type::Generic,
                         QStringLiteral("Unable to extract fast-filter fields from record %1 in block %2.")
                             .arg(static_cast<qulonglong>(localIndex + 1))
                             .arg(static_cast<qulonglong>(blockIndex)));
            records.clear();
            return false;
        }

        info.timestamp = static_cast<qint64>(recordTimestamps[localIndex]);
        records.push_back(info);
    }

    return true;
}

bool CLogBTraceLoader::buildBlockOptionalFieldRecords(
    const CLogBTraceMetadata& metadata,
    const std::size_t blockIndex,
    const CLog::CLogB::TagCHIRecords& block,
    const QString& fieldName,
    std::vector<CLogBTraceOptionalFieldValue>& records,
    CLogBTraceLoadError& error,
    const std::size_t maxWorkerCount,
    const CLogBTraceLoadStageProgressCallback& progressCallback,
    std::stop_token stopToken)
{
    records.clear();
    error = {};

    if (blockIndex >= metadata.blocks.size()) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("Requested block index %1 is out of range.")
                         .arg(static_cast<qulonglong>(blockIndex)));
        return false;
    }

    if (fieldName.isEmpty()) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("Optional field name is empty."));
        return false;
    }

    if (stopToken.stop_requested()) {
        setCancelledLoadError(error);
        return false;
    }

    const std::size_t clampedWorkerCount = std::clamp<std::size_t>(maxWorkerCount, 1, 8);
    switch (metadata.parameters.GetIssue()) {
    case CLog::Issue::Eb: {
        EbMeasures measures;
        if (!prepareEbTraceDecoding(metadata.parameters, measures, error)) {
            return false;
        }
        return buildEbOptionalFieldRecords(metadata.parameters,
                                           measures,
                                           block,
                                           fieldName,
                                           records,
                                           error,
                                           clampedWorkerCount,
                                           progressCallback,
                                           stopToken);
    }
    case CLog::Issue::B: {
        BMeasures measures;
        if (!prepareBTraceDecoding(metadata.parameters, measures, error)) {
            return false;
        }
        return buildBOptionalFieldRecords(metadata.parameters,
                                          measures,
                                          block,
                                          fieldName,
                                          records,
                                          error,
                                          clampedWorkerCount,
                                          progressCallback,
                                          stopToken);
    }
    default:
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("This CLog.B file uses unsupported CHI Issue %1.")
                         .arg(issueName(metadata.parameters.GetIssue())),
                     QStringLiteral("CloganGUI currently decodes CHI Issue B and CHI Issue E.b traces."),
                     describeParametersBlock(metadata.parameters));
        return false;
    }
}

bool CLogBTraceLoader::buildXactionIndex(const QString& filePath,
                                         const CLogBTraceMetadata& metadata,
                                         std::vector<CLogBTraceXactionIndexRecord>& records,
                                         CLogBTraceLoadError& error,
                                         const CLogBTraceLoadCallbacks& callbacks,
                                         std::stop_token stopToken)
{
    records.clear();
    if (metadata.totalRecords > static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("The trace is too large for in-memory Xaction indexing."));
        return false;
    }

    records.resize(static_cast<std::size_t>(metadata.totalRecords));
    const auto recordCallback =
        [&records](const std::uint64_t logicalRow, CLogBTraceXactionIndexRecord record) {
            if (logicalRow >= records.size()) {
                return false;
            }
            records[static_cast<std::size_t>(logicalRow)] = std::move(record);
            return true;
        };

    if (!buildXactionIndex(filePath, metadata, recordCallback, error, callbacks, stopToken)) {
        records.clear();
        return false;
    }

    reportStage(callbacks.stage,
                CLogBTraceLoadStage::Completed,
                1,
                static_cast<std::size_t>(records.size()));
    return true;
}

bool CLogBTraceLoader::buildXactionIndex(const QString& filePath,
                                         const CLogBTraceMetadata& metadata,
                                         const CLogBTraceXactionIndexRecordCallback& recordCallback,
                                         CLogBTraceLoadError& error,
                                         const CLogBTraceLoadCallbacks& callbacks,
                                         std::stop_token stopToken)
{
    error = {};

    if (stopToken.stop_requested()) {
        setCancelledLoadError(error);
        return false;
    }

    if (metadata.parameters.GetIssue() != CLog::Issue::Eb) {
        return true;
    }

    const QString resolvedFilePath = resolveTraceFilePath(filePath, metadata);
    if (resolvedFilePath.isEmpty()) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("No trace file path was provided for Xaction indexing."));
        return false;
    }

    EbMeasures measures;
    if (!prepareEbTraceDecoding(metadata.parameters, measures, error)) {
        return false;
    }

    const std::uint64_t totalRecords64 = metadata.totalRecords;
    const std::size_t totalRecords =
        static_cast<std::size_t>(std::min<std::uint64_t>(
            totalRecords64,
            static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)())));

    reportStage(callbacks.stage, CLogBTraceLoadStage::Parsing, 0, totalRecords);
    if (callbacks.stageProgress) {
        callbacks.stageProgress(0, totalRecords);
    }

    bool ok = false;
    switch (metadata.parameters.GetDataWidth()) {
    case 128:
        ok = buildEbXactionIndexRecords<EbXactionConfig<128>>(resolvedFilePath,
                                                              metadata,
                                                              measures,
                                                              recordCallback,
                                                              error,
                                                              callbacks,
                                                              stopToken);
        break;
    case 256:
        ok = buildEbXactionIndexRecords<EbXactionConfig<256>>(resolvedFilePath,
                                                              metadata,
                                                              measures,
                                                              recordCallback,
                                                              error,
                                                              callbacks,
                                                              stopToken);
        break;
    case 512:
    default:
        ok = buildEbXactionIndexRecords<EbXactionConfig<512>>(resolvedFilePath,
                                                              metadata,
                                                              measures,
                                                              recordCallback,
                                                              error,
                                                              callbacks,
                                                              stopToken);
        break;
    }

    if (!ok) {
        return false;
    }

    return true;
}

bool CLogBTraceLoader::buildCacheLineLifetimes(
    const QString& filePath,
    const CLogBTraceMetadata& metadata,
    std::vector<CLogBTraceCacheLineLifetime>& lifetimes,
    CLogBTraceLoadError& error,
    const CLogBTraceLoadCallbacks& callbacks,
    std::stop_token stopToken)
{
    lifetimes.clear();
    const auto lifetimeCallback = [&lifetimes](CLogBTraceCacheLineLifetime lifetime) {
        lifetimes.push_back(std::move(lifetime));
        return true;
    };
    if (!buildCacheLineLifetimes(filePath, metadata, lifetimeCallback, error, callbacks, stopToken)) {
        lifetimes.clear();
        return false;
    }
    std::sort(lifetimes.begin(), lifetimes.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.rnNodeId != rhs.rnNodeId) {
            return lhs.rnNodeId < rhs.rnNodeId;
        }
        if (lhs.startTimestamp != rhs.startTimestamp) {
            return lhs.startTimestamp < rhs.startTimestamp;
        }
        if (lhs.endTimestamp != rhs.endTimestamp) {
            return lhs.endTimestamp < rhs.endTimestamp;
        }
        return lhs.lineAddress < rhs.lineAddress;
    });
    return true;
}

bool CLogBTraceLoader::buildCacheLineLifetimes(
    const QString& filePath,
    const CLogBTraceMetadata& metadata,
    const CLogBTraceCacheLineLifetimeCallback& lifetimeCallback,
    CLogBTraceLoadError& error,
    const CLogBTraceLoadCallbacks& callbacks,
    std::stop_token stopToken)
{
    error = {};

    if (stopToken.stop_requested()) {
        setCancelledLoadError(error);
        return false;
    }

    if (metadata.parameters.GetIssue() != CLog::Issue::Eb) {
        return true;
    }

    const QString resolvedFilePath = resolveTraceFilePath(filePath, metadata);
    if (resolvedFilePath.isEmpty()) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("No trace file path was provided for Cache lifetime replay."));
        return false;
    }

    EbMeasures measures;
    if (!prepareEbTraceDecoding(metadata.parameters, measures, error)) {
        return false;
    }

    switch (metadata.parameters.GetDataWidth()) {
    case 128:
        return buildEbCacheLineLifetimes<EbXactionConfig<128>>(resolvedFilePath,
                                                               metadata,
                                                               measures,
                                                               lifetimeCallback,
                                                               error,
                                                               callbacks,
                                                               stopToken);
    case 256:
        return buildEbCacheLineLifetimes<EbXactionConfig<256>>(resolvedFilePath,
                                                               metadata,
                                                               measures,
                                                               lifetimeCallback,
                                                               error,
                                                               callbacks,
                                                               stopToken);
    case 512:
    default:
        return buildEbCacheLineLifetimes<EbXactionConfig<512>>(resolvedFilePath,
                                                               metadata,
                                                               measures,
                                                               lifetimeCallback,
                                                               error,
                                                               callbacks,
                                                               stopToken);
    }
}

bool CLogBTraceLoader::collectFastRecordRowsMatchingFilter(const CLogBTraceMetadata& metadata,
                                                           const CLogBTraceFastFilter& filter,
                                                           const std::vector<CLogBTraceFastRecordInfo>& records,
                                                           std::vector<std::size_t>& localRows,
                                                           CLogBTraceLoadError& error)
{
    error = {};
    collectMatchingLocalRows(0,
                             records.size(),
                             [&](const std::size_t localIndex) {
                                 return fastRecordMatchesFilter(metadata.parameters,
                                                                filter,
                                                                records[localIndex]);
                             },
                             localRows);

    return true;
}

bool CLogBTraceLoader::collectFastRecordRowsMatchingFilterRange(
    const CLogBTraceMetadata& metadata,
    const CLogBTraceFastFilter& filter,
    const std::vector<CLogBTraceFastRecordInfo>& records,
    const std::size_t localBegin,
    const std::size_t rowCount,
    std::vector<std::size_t>& localRows,
    CLogBTraceLoadError& error)
{
    error = {};

    if (localBegin >= records.size() || rowCount == 0) {
        localRows.clear();
        return true;
    }

    const std::size_t localEnd = std::min<std::size_t>(records.size(), localBegin + rowCount);
    collectMatchingLocalRows(localBegin,
                             localEnd,
                             [&](const std::size_t localIndex) {
                                 return fastRecordMatchesFilter(metadata.parameters,
                                                                filter,
                                                                records[localIndex]);
                             },
                             localRows);

    return true;
}

bool CLogBTraceLoader::loadRows(const QString& filePath,
                                const CLogBTraceMetadata& metadata,
                                const std::uint64_t beginRow,
                                const std::uint64_t rowCount,
                                std::vector<FlitRecord>& records,
                                CLogBTraceLoadError& error,
                                const CLogBTraceLoadCallbacks& callbacks,
                                std::stop_token stopToken)
{
    records.clear();
    error = {};

    reportStage(callbacks.stage, CLogBTraceLoadStage::Opening, 0, 0);

    if (beginRow > metadata.totalRecords) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("Requested row range starts beyond the end of the trace."));
        return false;
    }

    const std::uint64_t clampedRowCount = std::min<std::uint64_t>(rowCount, metadata.totalRecords - beginRow);
    if (clampedRowCount == 0) {
        reportProgress(callbacks.progress, 0, 0);
        reportStage(callbacks.stage, CLogBTraceLoadStage::Completed, 0, 0);
        return true;
    }

    const QString resolvedFilePath = resolveTraceFilePath(filePath, metadata);
    if (resolvedFilePath.isEmpty()) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("No trace file path was provided for row loading."));
        return false;
    }

    std::vector<std::size_t> selectedBlockIndexes;
    std::uint64_t selectedSerializedBytes = 0;
    const std::uint64_t endRow = beginRow + clampedRowCount;
    for (std::size_t index = 0; index < metadata.blocks.size(); ++index) {
        const CLogBTraceBlockSummary& block = metadata.blocks[index];
        const std::uint64_t blockEndRow = block.rowStart + block.recordCount;
        if (blockEndRow <= beginRow || block.rowStart >= endRow) {
            continue;
        }

        selectedBlockIndexes.push_back(index);
        selectedSerializedBytes += block.serializedSize;
    }

    reportProgress(callbacks.progress, 0, selectedSerializedBytes);
    std::uint64_t processedBytes = 0;
    for (const std::size_t blockIndex : selectedBlockIndexes) {
        if (stopToken.stop_requested()) {
            setCancelledLoadError(error);
            return false;
        }

        const CLogBTraceBlockSummary& block = metadata.blocks[blockIndex];
        std::shared_ptr<CLog::CLogB::TagCHIRecords> fullTag;
        if (!loadBlockRecords(resolvedFilePath,
                              metadata,
                              blockIndex,
                              fullTag,
                              error,
                              callbacks,
                              stopToken)) {
            return false;
        }

        const std::uint64_t overlapBegin = std::max(beginRow, block.rowStart);
        const std::uint64_t overlapEnd = std::min(endRow, block.rowStart + block.recordCount);
        const std::size_t localBegin = static_cast<std::size_t>(overlapBegin - block.rowStart);
        const std::size_t localCount = static_cast<std::size_t>(overlapEnd - overlapBegin);
        const bool trackXactions = false;
        if (!decodeTagSlice(metadata,
                            blockIndex,
                            *fullTag,
                            localBegin,
                            localCount,
                            records,
                            error,
                            callbacks,
                            stopToken,
                            true,
                            trackXactions)) {
            return false;
        }

        processedBytes += block.serializedSize;
        reportProgress(callbacks.progress, processedBytes, selectedSerializedBytes);
        reportStage(callbacks.stage, CLogBTraceLoadStage::Parsing, 0, 0);
    }

    reportProgress(callbacks.progress, selectedSerializedBytes, selectedSerializedBytes);
    reportStage(callbacks.stage, CLogBTraceLoadStage::Completed, 0, 0);
    return true;
}

bool CLogBTraceLoader::loadFile(const QString& filePath,
                                std::vector<FlitRecord>& records,
                                CLogBTraceLoadError& error,
                                const CLogBTraceLoadCallbacks& callbacks,
                                std::stop_token stopToken)
{
    records.clear();
    error = {};

    reportStage(callbacks.stage, CLogBTraceLoadStage::Opening, 0, 0);

    const std::filesystem::path path(filePath.toStdWString());
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("Unable to open %1.")
                         .arg(QFileInfo(filePath).fileName()));
        return false;
    }

    std::vector<char> streamBuffer(4U << 20);
    input.rdbuf()->pubsetbuf(streamBuffer.data(), static_cast<std::streamsize>(streamBuffer.size()));

    std::error_code fileSizeError;
    const std::uint64_t totalBytes = std::filesystem::file_size(path, fileSizeError);
    reportProgress(callbacks.progress, 0, fileSizeError ? 0U : totalBytes);
    reportStage(callbacks.stage, CLogBTraceLoadStage::Parsing, 0, 0);

    CLog::CLogB::Reader reader;
    CLog::Parameters params;
    EbMeasures ebMeasures;
    BMeasures bMeasures;
    bool hasParameters = false;
    bool decodingPrepared = false;
    std::unordered_map<quint16, QString> nodeAnnotations;
    std::unordered_map<quint16, CLog::NodeType> nodeTypes;
    std::vector<FlitRecord> parsedRows;
    std::uint64_t processedBytes = 0;

    while (true) {
        if (stopToken.stop_requested()) {
            setCancelledLoadError(error);
            return false;
        }

        const std::uint64_t tagStartBytes = streamBytePosition(input, processedBytes);
        std::string readError;
        std::shared_ptr<CLog::CLogB::Tag> tag = reader.Next(input, readError);
        const std::uint64_t tagEndBytes = streamBytePosition(input, processedBytes);

        if (stopToken.stop_requested()) {
            setCancelledLoadError(error);
            return false;
        }

        if (!tag) {
            setLoadError(error,
                         CLogBTraceLoadError::Type::Generic,
                         QStringLiteral("CLog.B parsing failed."),
                         QStringLiteral("The file could not be read as a valid CLog.B stream."),
                         QString::fromStdString(readError));
            return false;
        }

        if (tag->type == CLog::CLogB::Encodings::_EOF) {
            reportProgress(callbacks.progress, fileSizeError ? processedBytes : totalBytes, fileSizeError ? 0U : totalBytes);
            break;
        }

        switch (tag->type) {
        case CLog::CLogB::Encodings::CHI_PARAMETERS:
            params = std::static_pointer_cast<CLog::CLogB::TagCHIParameters>(tag)->parameters;
            hasParameters = true;
            decodingPrepared = false;
            processedBytes = tagEndBytes;
            reportProgress(callbacks.progress, processedBytes, fileSizeError ? 0U : totalBytes);
            break;

        case CLog::CLogB::Encodings::CHI_TOPOS: {
            const auto topologies = std::static_pointer_cast<CLog::CLogB::TagCHITopologies>(tag);
            for (const auto& node : topologies->nodes) {
                nodeAnnotations.insert_or_assign(
                    node.id,
                    InternDisplayString(
                        QStringLiteral("Captured at node %1 (%2).")
                            .arg(node.id)
                            .arg(QString::fromStdString(CLog::NodeTypeToString(node.type)))));
                nodeTypes.insert_or_assign(node.id, node.type);
            }
            processedBytes = tagEndBytes;
            reportProgress(callbacks.progress, processedBytes, fileSizeError ? 0U : totalBytes);
            break;
        }

        case CLog::CLogB::Encodings::CHI_RECORDS:
            if (!hasParameters) {
                setLoadError(error,
                             CLogBTraceLoadError::Type::Generic,
                             QStringLiteral("CLog.B file is missing CHI parameters."),
                             QStringLiteral("The first record block appeared before the file declared its CHI configuration."));
                return false;
            }

            if (!decodingPrepared) {
                switch (params.GetIssue()) {
                case CLog::Issue::Eb:
                    if (!prepareEbTraceDecoding(params, ebMeasures, error)) {
                        return false;
                    }
                    break;
                case CLog::Issue::B:
                    if (!prepareBTraceDecoding(params, bMeasures, error)) {
                        return false;
                    }
                    break;
                default:
                    setLoadError(error,
                                 CLogBTraceLoadError::Type::Generic,
                                 QStringLiteral("This CLog.B file uses unsupported CHI Issue %1.")
                                     .arg(issueName(params.GetIssue())),
                                 QStringLiteral("CloganGUI currently decodes CHI Issue B and CHI Issue E.b traces."),
                                 describeParametersBlock(params));
                    return false;
                }
                decodingPrepared = true;
            }

            switch (params.GetIssue()) {
            case CLog::Issue::Eb:
                if (!appendDecodedTag(params,
                                      ebMeasures,
                                      *std::static_pointer_cast<CLog::CLogB::TagCHIRecords>(tag),
                                      nodeAnnotations,
                                      nodeTypes,
                                      parsedRows,
                                      error,
                                      callbacks,
                                      tagStartBytes,
                                      tagEndBytes,
                                      fileSizeError ? 0U : totalBytes,
                                      stopToken)) {
                    return false;
                }
                break;
            case CLog::Issue::B:
                if (!appendDecodedBTag(params,
                                       bMeasures,
                                       *std::static_pointer_cast<CLog::CLogB::TagCHIRecords>(tag),
                                       nodeAnnotations,
                                       parsedRows,
                                       error,
                                       callbacks,
                                       tagStartBytes,
                                       tagEndBytes,
                                       fileSizeError ? 0U : totalBytes,
                                       stopToken)) {
                    return false;
                }
                break;
            default:
                return false;
            }
            processedBytes = tagEndBytes;
            reportProgress(callbacks.progress, processedBytes, fileSizeError ? 0U : totalBytes);
            reportStage(callbacks.stage, CLogBTraceLoadStage::Parsing, 0, 0);
            break;

        default:
            setLoadError(error,
                         CLogBTraceLoadError::Type::Generic,
                         QStringLiteral("Unsupported CLog.B tag type 0x%1.")
                            .arg(QString::number(static_cast<int>(tag->type), 16)
                                     .toUpper()
                                     .rightJustified(2, QLatin1Char('0'))));
            return false;
        }
    }

    if (!hasParameters) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("CLog.B file does not contain a CHI parameters tag."));
        return false;
    }

    records = std::move(parsedRows);
    reportProgress(callbacks.progress, fileSizeError ? processedBytes : totalBytes, fileSizeError ? 0U : totalBytes);
    reportStage(callbacks.stage, CLogBTraceLoadStage::Completed, 0, 0);
    return true;
}

bool CLogBTraceLoader::saveRowsAsCLogB(const QString& filePath,
                                       const CLogBTraceMetadata& metadata,
                                       const std::vector<FlitRecord>& records,
                                       CLogBTraceLoadError& error)
{
    return saveRowsAsCLogB(filePath,
                           metadata,
                           static_cast<std::uint64_t>(records.size()),
                           [&records](const std::uint64_t rowIndex,
                                      FlitRecord& record,
                                      CLogBTraceLoadError&) {
                               if (rowIndex >= records.size()) {
                                   return false;
                               }
                               record = records[static_cast<std::size_t>(rowIndex)];
                               return true;
                           },
                           error);
}

bool CLogBTraceLoader::saveRowsAsCLogB(
    const QString& filePath,
    const CLogBTraceMetadata& metadata,
    const std::uint64_t rowCount,
    const std::function<bool(std::uint64_t rowIndex, FlitRecord& record, CLogBTraceLoadError& error)>& rowProvider,
    CLogBTraceLoadError& error)
{
    error = {};
    if (!rowProvider && rowCount != 0) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("No edited row provider is available for saving."));
        return false;
    }

    const std::filesystem::path path(filePath.toStdWString());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("Unable to create %1.")
                         .arg(QFileInfo(filePath).fileName()),
                     QStringLiteral("The edited trace copy could not be opened for writing."));
        return false;
    }

    CLog::CLogB::Writer writer;

    CLog::CLogB::TagCHIParameters parameterTag;
    parameterTag.parameters = metadata.parameters;
    writer.Next(output, &parameterTag);

    if (!metadata.nodeTypes.empty()) {
        CLog::CLogB::TagCHITopologies topologyTag;
        topologyTag.nodes.reserve(metadata.nodeTypes.size());
        for (const auto& [nodeId, nodeType] : metadata.nodeTypes) {
            topologyTag.nodes.push_back(CLog::CLogB::TagCHITopologies::Node{
                .type = nodeType,
                .id = nodeId,
            });
        }
        std::sort(topologyTag.nodes.begin(),
                  topologyTag.nodes.end(),
                  [](const auto& lhs, const auto& rhs) {
                      return lhs.id < rhs.id;
                  });
        writer.Next(output, &topologyTag);
    }

    constexpr std::size_t kRecordsPerBlock = 4096;
    std::uint64_t rowIndex = 0;
    while (rowIndex < rowCount) {
        const std::uint64_t blockBegin = rowIndex;
        const std::uint64_t blockEnd = std::min<std::uint64_t>(rowCount, blockBegin + kRecordsPerBlock);

        FlitRecord row;
        if (!rowProvider(blockBegin, row, error)) {
            if (error.summary.isEmpty()) {
                setLoadError(error,
                             CLogBTraceLoadError::Type::Generic,
                             QStringLiteral("Edited row %1 could not be read for saving.")
                                 .arg(static_cast<qulonglong>(blockBegin + 1)));
            }
            return false;
        }
        if (row.timestamp < 0) {
            setLoadError(error,
                         CLogBTraceLoadError::Type::Generic,
                         QStringLiteral("Edited row %1 has a negative timestamp.")
                             .arg(static_cast<qulonglong>(blockBegin + 1)));
            return false;
        }

        CLog::CLogB::TagCHIRecords recordTag;
        recordTag.head.timeBase = static_cast<std::uint64_t>(row.timestamp);
        recordTag.head.compressed = 1;
        recordTag.records.reserve(static_cast<std::size_t>(blockEnd - blockBegin));

        qint64 previousTimestamp = row.timestamp;
        for (; rowIndex < blockEnd; ++rowIndex) {
            if (rowIndex != blockBegin
                && !rowProvider(rowIndex, row, error)) {
                if (error.summary.isEmpty()) {
                    setLoadError(error,
                                 CLogBTraceLoadError::Type::Generic,
                                 QStringLiteral("Edited row %1 could not be read for saving.")
                                     .arg(static_cast<qulonglong>(rowIndex + 1)));
                }
                return false;
            }
            if (!row.rawRecordAvailable
                || row.rawFlitLength == 0
                || row.rawFlitWords.empty()) {
                setLoadError(error,
                             CLogBTraceLoadError::Type::Generic,
                             QStringLiteral("Edited row %1 cannot be serialized.")
                                 .arg(static_cast<qulonglong>(rowIndex + 1)),
                             QStringLiteral("This row has no raw CLog.B flit payload."));
                return false;
            }
            if (row.timestamp < previousTimestamp) {
                setLoadError(error,
                             CLogBTraceLoadError::Type::Generic,
                             QStringLiteral("Edited row %1 is out of timestamp order.")
                                 .arg(static_cast<qulonglong>(rowIndex + 1)),
                             QStringLiteral("Save Edited Copy requires nondecreasing row timestamps."));
                return false;
            }
            const std::uint64_t delta = static_cast<std::uint64_t>(row.timestamp - previousTimestamp);
            if (delta > std::numeric_limits<std::uint32_t>::max()) {
                setLoadError(error,
                             CLogBTraceLoadError::Type::Generic,
                             QStringLiteral("Edited row %1 has a timestamp delta too large for CLog.B.")
                                 .arg(static_cast<qulonglong>(rowIndex + 1)));
                return false;
            }

            const std::size_t wordCount = (static_cast<std::size_t>(row.rawFlitLength) + 3U) / 4U;
            if (row.rawFlitWords.size() < wordCount) {
                setLoadError(error,
                             CLogBTraceLoadError::Type::Generic,
                             QStringLiteral("Edited row %1 has a truncated raw flit payload.")
                                 .arg(static_cast<qulonglong>(rowIndex + 1)));
                return false;
            }

            CLog::CLogB::TagCHIRecords::Record record;
            record.timeShift = static_cast<std::uint32_t>(delta);
            record.nodeId = row.rawNodeId;
            record.channel = static_cast<CLog::Channel>(row.rawChannel);
            record.flitLength = row.rawFlitLength;
            record.flit = std::shared_ptr<std::uint32_t[]>(new std::uint32_t[wordCount]());
            std::copy_n(row.rawFlitWords.data(), wordCount, record.flit.get());
            recordTag.records.push_back(std::move(record));
            previousTimestamp = row.timestamp;
        }

        writer.Next(output, &recordTag);
    }

    CLog::CLogB::TagEOF eofTag;
    writer.Next(output, &eofTag);

    if (!output.good()) {
        setLoadError(error,
                     CLogBTraceLoadError::Type::Generic,
                     QStringLiteral("Writing %1 failed.")
                         .arg(QFileInfo(filePath).fileName()),
                     QStringLiteral("The output stream reported an error while writing the edited trace copy."));
        return false;
    }

    return true;
}

void CLogBTraceLoader::prepareFastFilter(CLogBTraceFastFilter& filter)
{
    for (auto& values : filter.opcodeValuesByChannel) {
        values.clear();
    }

    const QString opcodeFilter = filter.opcodeFilter.trimmed();
    if (opcodeFilter.isEmpty()) {
        return;
    }

    qulonglong ignored = 0;
    if (parseSearchIntegerFast(opcodeFilter, ignored)) {
        return;
    }

    appendOpcodeMatchesForChannels<CHI::Eb::Opcodes::REQ::Decoder<FlexibleReqFlit>>(
        opcodeFilter,
        filter.opcodeValuesByChannel[static_cast<unsigned int>(CLog::Channel::TXREQ)],
        filter.opcodeValuesByChannel[static_cast<unsigned int>(CLog::Channel::RXREQ)],
        filter.opcodeValuesByChannel[static_cast<unsigned int>(CLog::Channel::TXREQ_BeforeSAM)],
        filter.opcodeValuesByChannel[static_cast<unsigned int>(CLog::Channel::RXREQ_BeforeSAM)]);
    appendOpcodeMatchesForChannels<CHI::Eb::Opcodes::RSP::Decoder<FlexibleRspFlit>>(
        opcodeFilter,
        filter.opcodeValuesByChannel[static_cast<unsigned int>(CLog::Channel::TXRSP)],
        filter.opcodeValuesByChannel[static_cast<unsigned int>(CLog::Channel::RXRSP)]);
    appendOpcodeMatchesForChannels<CHI::Eb::Opcodes::DAT::Decoder<FlexibleDatFlit>>(
        opcodeFilter,
        filter.opcodeValuesByChannel[static_cast<unsigned int>(CLog::Channel::TXDAT)],
        filter.opcodeValuesByChannel[static_cast<unsigned int>(CLog::Channel::RXDAT)]);
    appendOpcodeMatchesForChannels<CHI::Eb::Opcodes::SNP::Decoder<FlexibleSnpFlit>>(
        opcodeFilter,
        filter.opcodeValuesByChannel[static_cast<unsigned int>(CLog::Channel::TXSNP)],
        filter.opcodeValuesByChannel[static_cast<unsigned int>(CLog::Channel::RXSNP)]);
    appendOpcodeMatchesForChannels<CHI::B::Opcodes::REQ::Decoder<FlexibleBReqFlit>>(
        opcodeFilter,
        filter.opcodeValuesByChannel[static_cast<unsigned int>(CLog::Channel::TXREQ)],
        filter.opcodeValuesByChannel[static_cast<unsigned int>(CLog::Channel::RXREQ)],
        filter.opcodeValuesByChannel[static_cast<unsigned int>(CLog::Channel::TXREQ_BeforeSAM)],
        filter.opcodeValuesByChannel[static_cast<unsigned int>(CLog::Channel::RXREQ_BeforeSAM)]);
    appendOpcodeMatchesForChannels<CHI::B::Opcodes::RSP::Decoder<FlexibleBRspFlit>>(
        opcodeFilter,
        filter.opcodeValuesByChannel[static_cast<unsigned int>(CLog::Channel::TXRSP)],
        filter.opcodeValuesByChannel[static_cast<unsigned int>(CLog::Channel::RXRSP)]);
    appendOpcodeMatchesForChannels<CHI::B::Opcodes::DAT::Decoder<FlexibleBDatFlit>>(
        opcodeFilter,
        filter.opcodeValuesByChannel[static_cast<unsigned int>(CLog::Channel::TXDAT)],
        filter.opcodeValuesByChannel[static_cast<unsigned int>(CLog::Channel::RXDAT)]);
    appendOpcodeMatchesForChannels<CHI::B::Opcodes::SNP::Decoder<FlexibleBSnpFlit>>(
        opcodeFilter,
        filter.opcodeValuesByChannel[static_cast<unsigned int>(CLog::Channel::TXSNP)],
        filter.opcodeValuesByChannel[static_cast<unsigned int>(CLog::Channel::RXSNP)]);

    for (auto& values : filter.opcodeValuesByChannel) {
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end()), values.end());
    }
}

bool CLogBTraceLoader::guessFlitConfigurations(const QString& filePath,
                                               CLogBTraceConfigurationGuessResult& result,
                                               CLogBTraceLoadError& error,
                                               const CLogBTraceLoadCallbacks& callbacks,
                                               std::stop_token stopToken)
{
    result = {};
    error = {};

    CLogBTraceMetadata metadata;
    if (!scanFile(filePath, metadata, error, callbacks, stopToken)) {
        return false;
    }

    result.declaredParameters = metadata.parameters;
    result.totalRecords = metadata.totalRecords;
    for (std::size_t channelIndex = 0; channelIndex < metadata.flitLengthMasks.size(); ++channelIndex) {
        result.observedByteLengths[channelIndex] =
            flitLengthMaskValues(metadata.flitLengthMasks[channelIndex]);
    }

    const bool hasObservedFlitLength =
        std::any_of(metadata.flitLengthMasks.begin(),
                    metadata.flitLengthMasks.end(),
                    [](const CLogBTraceFlitLengthMask& mask) {
                        return !flitLengthMaskIsEmpty(mask);
                    });
    if (!hasObservedFlitLength) {
        return true;
    }

    constexpr std::array<std::size_t, 5> kNodeIdWidths{7, 8, 9, 10, 11};
    constexpr std::array<std::size_t, 9> kReqAddrWidths{44, 45, 46, 47, 48, 49, 50, 51, 52};
    constexpr std::array<std::size_t, 7> kRsvdcWidths{0, 4, 8, 12, 16, 24, 32};
    constexpr std::array<std::size_t, 3> kDataWidths{128, 256, 512};
    constexpr std::array<bool, 2> kBooleanValues{false, true};

    auto appendCandidate = [&](const CLog::Parameters& params) {
        CLogBTraceLoadError measureError;
        switch (params.GetIssue()) {
        case CLog::Issue::Eb: {
            EbMeasures measures;
            if (buildEbMeasures(params, measures, measureError)) {
                appendMatchingGuess(metadata.flitLengthMasks,
                                    params,
                                    expectedBitWidthsFor(measures),
                                    expectedByteLengthsFor(measures),
                                    result.guesses);
            }
            break;
        }
        case CLog::Issue::B: {
            BMeasures measures;
            if (buildBMeasures(params, measures, measureError)) {
                appendMatchingGuess(metadata.flitLengthMasks,
                                    params,
                                    expectedBitWidthsFor(measures),
                                    expectedByteLengthsFor(measures),
                                    result.guesses);
            }
            break;
        }
        }
    };

    for (const CLog::Issue issue : {CLog::Issue::B, CLog::Issue::Eb}) {
        for (const std::size_t nodeIdWidth : kNodeIdWidths) {
            for (const std::size_t reqAddrWidth : kReqAddrWidths) {
                for (const std::size_t reqRsvdcWidth : kRsvdcWidths) {
                    for (const std::size_t datRsvdcWidth : kRsvdcWidths) {
                        for (const std::size_t dataWidth : kDataWidths) {
                            for (const bool dataCheckPresent : kBooleanValues) {
                                for (const bool poisonPresent : kBooleanValues) {
                                    for (const bool mpamPresent : kBooleanValues) {
                                        if (stopToken.stop_requested()) {
                                            setCancelledLoadError(error);
                                            result.guesses.clear();
                                            return false;
                                        }
                                        if (issue == CLog::Issue::B && mpamPresent) {
                                            continue;
                                        }

                                        CLog::Parameters params;
                                        params.SetIssue(issue);
                                        if (!params.SetNodeIdWidth(nodeIdWidth)
                                            || !params.SetReqAddrWidth(reqAddrWidth)
                                            || !params.SetReqRSVDCWidth(reqRsvdcWidth)
                                            || !params.SetDatRSVDCWidth(datRsvdcWidth)
                                            || !params.SetDataWidth(dataWidth)) {
                                            continue;
                                        }
                                        params.SetDataCheckPresent(dataCheckPresent);
                                        params.SetPoisonPresent(poisonPresent);
                                        params.SetMPAMPresent(issue == CLog::Issue::Eb && mpamPresent);
                                        appendCandidate(params);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    std::sort(result.guesses.begin(),
              result.guesses.end(),
              [&](const CLogBTraceConfigurationGuess& lhs,
                  const CLogBTraceConfigurationGuess& rhs) {
                  return parameterSortLess(lhs, rhs, metadata.parameters);
              });
    return true;
}

QString CLogBTraceLoader::supportedConfigurationSummary()
{
    return QStringLiteral(
        "Any legal CHI Issue B or CHI Issue E.b NodeIDWidth, ReqAddrWidth, ReqRSVDCWidth, DatRSVDCWidth, and DataWidth values. For Issue B, DataCheck and Poison may be enabled or disabled. For Issue E.b, DataCheck, Poison, and MPAM may be enabled or disabled.");
}

QStringList CLogBTraceLoader::supportedOpcodeNames()
{
    QStringList names;
    QSet<QString> seen;

    appendSupportedOpcodeNames<CHI::Eb::Opcodes::REQ::Decoder<FlexibleReqFlit>>(names, seen);
    appendSupportedOpcodeNames<CHI::Eb::Opcodes::RSP::Decoder<FlexibleRspFlit>>(names, seen);
    appendSupportedOpcodeNames<CHI::Eb::Opcodes::DAT::Decoder<FlexibleDatFlit>>(names, seen);
    appendSupportedOpcodeNames<CHI::Eb::Opcodes::SNP::Decoder<FlexibleSnpFlit>>(names, seen);
    appendSupportedOpcodeNames<CHI::B::Opcodes::REQ::Decoder<FlexibleBReqFlit>>(names, seen);
    appendSupportedOpcodeNames<CHI::B::Opcodes::RSP::Decoder<FlexibleBRspFlit>>(names, seen);
    appendSupportedOpcodeNames<CHI::B::Opcodes::DAT::Decoder<FlexibleBDatFlit>>(names, seen);
    appendSupportedOpcodeNames<CHI::B::Opcodes::SNP::Decoder<FlexibleBSnpFlit>>(names, seen);

    std::sort(names.begin(), names.end(), [](const QString& lhs, const QString& rhs) {
        return lhs.compare(rhs, Qt::CaseInsensitive) < 0;
    });
    return names;
}

}  // namespace CHIron::Gui
