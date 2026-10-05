#include "node_type_guess.hpp"

#include "chiron_eb_expresso.hpp"

#include <limits>

namespace CHIron::Gui {
namespace {

constexpr std::uint32_t kInvalidId = std::numeric_limits<std::uint32_t>::max();

bool isValidNodeId(std::uint32_t id)
{
    return id != kInvalidId && id <= std::numeric_limits<quint16>::max();
}

bool isReqChannel(CLog::Channel channel)
{
    switch (channel) {
    case CLog::Channel::TXREQ:
    case CLog::Channel::RXREQ:
    case CLog::Channel::TXREQ_BeforeSAM:
    case CLog::Channel::RXREQ_BeforeSAM:
        return true;
    default:
        return false;
    }
}

bool isTxChannel(CLog::Channel channel)
{
    switch (channel) {
    case CLog::Channel::TXREQ:
    case CLog::Channel::TXRSP:
    case CLog::Channel::TXDAT:
    case CLog::Channel::TXSNP:
    case CLog::Channel::TXREQ_BeforeSAM:
        return true;
    default:
        return false;
    }
}

bool isSnpResponseDataOpcode(std::uint32_t opcode)
{
    namespace Opcodes = CHI::Eb::Opcodes;
    return opcode == Opcodes::DAT::SnpRespData
        || opcode == Opcodes::DAT::SnpRespDataPtl
        || opcode == Opcodes::DAT::SnpRespDataFwded;
}

bool isSnpResponseOpcode(std::uint32_t opcode)
{
    namespace Opcodes = CHI::Eb::Opcodes;
    return opcode == Opcodes::RSP::SnpResp
        || opcode == Opcodes::RSP::SnpRespFwded;
}

// WriteBack/WriteEvict/WriteClean requests move data out of a coherent cache;
// only RN-F nodes issue them.
bool isCachedWriteOpcode(std::uint32_t opcode)
{
    namespace Opcodes = CHI::Eb::Opcodes;
    return opcode == Opcodes::REQ::WriteBackFull
        || opcode == Opcodes::REQ::WriteBackPtl
        || opcode == Opcodes::REQ::WriteEvictFull
        || opcode == Opcodes::REQ::WriteEvictOrEvict
        || opcode == Opcodes::REQ::WriteBackFullCleanSh
        || opcode == Opcodes::REQ::WriteBackFullCleanInv
        || opcode == Opcodes::REQ::WriteBackFullCleanShPerSep
        || opcode == Opcodes::REQ::WriteCleanFull
        || opcode == Opcodes::REQ::WriteCleanFullCleanSh
        || opcode == Opcodes::REQ::WriteCleanFullCleanShPerSep;
}

CLog::NodeType beforeSamVariantOf(const CLog::NodeType type) noexcept
{
    switch (type) {
    case CLog::NodeType::RN_F:
        return CLog::NodeType::RN_F_BeforeSAM;
    case CLog::NodeType::RN_D:
        return CLog::NodeType::RN_D_BeforeSAM;
    case CLog::NodeType::RN_I:
        return CLog::NodeType::RN_I_BeforeSAM;
    default:
        return type;
    }
}

}  // namespace

void NodeTypeGuesser::clear() noexcept
{
    evidence_.clear();
}

void NodeTypeGuesser::observe(const CLogBTraceFastRecordInfo& info)
{
    const CLog::Channel channel = static_cast<CLog::Channel>(info.channel);
    const bool tx = isTxChannel(channel);
    const bool hasCaptureNode = isValidNodeId(info.nodeId);
    const bool hasSrc = isValidNodeId(info.sourceId);
    const bool hasTgt = isValidNodeId(info.targetId);

    const auto captureId = static_cast<quint16>(info.nodeId);
    const auto srcId = static_cast<quint16>(info.sourceId);
    const auto tgtId = static_cast<quint16>(info.targetId);

    if (isReqChannel(channel)) {
        const bool beforeSam = channel == CLog::Channel::TXREQ_BeforeSAM
            || channel == CLog::Channel::RXREQ_BeforeSAM;
        if (tx) {
            if (hasCaptureNode) {
                NodeTrafficEvidence& node = evidence_[captureId];
                node.sendsReq = true;
                if (beforeSam) {
                    node.sendsReqBeforeSAM = true;
                } else {
                    node.sendsReqAfterSAM = true;
                }
            }
        } else {
            if (hasCaptureNode) {
                evidence_[captureId].recvReq = true;
            }
        }
        if (hasSrc) {
            NodeTrafficEvidence& src = evidence_[srcId];
            src.sendsReq = true;
            if (beforeSam) {
                src.sendsReqBeforeSAM = true;
            } else {
                src.sendsReqAfterSAM = true;
            }
            if (info.opcode == CHI::Eb::Opcodes::REQ::PrefetchTgt) {
                src.sendsPrefetchTgt = true;
            }
            if (isCachedWriteOpcode(info.opcode)) {
                src.sendsCachedWriteData = true;
            }
        }
        if (hasTgt) {
            NodeTrafficEvidence& tgt = evidence_[tgtId];
            tgt.recvReq = true;
            if (hasSrc) {
                tgt.incomingReqSources.insert(srcId);
            }
        }
        return;
    }

    switch (channel) {
    case CLog::Channel::TXSNP:
        if (hasCaptureNode) {
            evidence_[captureId].sendsSnp = true;
        }
        if (hasSrc) {
            evidence_[srcId].sendsSnp = true;
        }
        return;
    case CLog::Channel::RXSNP:
        // Only requesters ever receive snoops; the capture node is the target
        // because SNP flits carry no TgtID field.
        if (hasCaptureNode) {
            evidence_[captureId].recvSnp = true;
        }
        // The snoop source is the Home (high confidence HN-F).
        if (hasSrc) {
            evidence_[srcId].sendsSnp = true;
        }
        return;
    case CLog::Channel::TXRSP:
    case CLog::Channel::RXRSP: {
        const bool snpResp = isSnpResponseOpcode(info.opcode);
        if (tx) {
            if (hasCaptureNode) {
                NodeTrafficEvidence& node = evidence_[captureId];
                node.sendsRsp = true;
                node.sendsSnpResp = node.sendsSnpResp || snpResp;
            }
            if (hasSrc) {
                NodeTrafficEvidence& src = evidence_[srcId];
                src.sendsRsp = true;
                src.sendsSnpResp = src.sendsSnpResp || snpResp;
            }
        } else {
            if (hasCaptureNode) {
                evidence_[captureId].recvRsp = true;
            }
            if (hasTgt) {
                evidence_[tgtId].recvRsp = true;
            }
        }
        return;
    }
    case CLog::Channel::TXDAT:
    case CLog::Channel::RXDAT: {
        const bool snpData = isSnpResponseDataOpcode(info.opcode);
        if (tx) {
            if (hasCaptureNode) {
                NodeTrafficEvidence& node = evidence_[captureId];
                node.sendsDat = true;
                node.sendsSnpData = node.sendsSnpData || snpData;
            }
            if (hasSrc) {
                NodeTrafficEvidence& src = evidence_[srcId];
                src.sendsDat = true;
                src.sendsSnpData = src.sendsSnpData || snpData;
            }
        } else {
            if (hasCaptureNode) {
                evidence_[captureId].recvDat = true;
            }
            if (hasTgt) {
                evidence_[tgtId].recvDat = true;
            }
        }
        return;
    }
    default:
        return;
    }
}

std::vector<NodeTypeGuess> NodeTypeGuesser::results(const CLogBTraceMetadata& metadata) const
{
    std::set<quint16> nodeIds;
    for (const auto& [nodeId, unused] : evidence_) {
        (void) unused;
        nodeIds.insert(nodeId);
    }
    for (const auto& [nodeId, unused] : metadata.nodeTypes) {
        (void) unused;
        nodeIds.insert(nodeId);
    }

    const auto isHomeLike = [this](quint16 nodeId) {
        const auto found = evidence_.find(nodeId);
        return found != evidence_.cend()
            && (found->second.recvReq || found->second.sendsSnp);
    };

    std::vector<NodeTypeGuess> guesses;
    guesses.reserve(nodeIds.size());

    for (const quint16 nodeId : nodeIds) {
        NodeTypeGuess guess;
        guess.nodeId = nodeId;

        const auto fileIt = metadata.nodeTypes.find(nodeId);
        guess.mappedInFile = fileIt != metadata.nodeTypes.cend();
        if (guess.mappedInFile) {
            guess.fileType = fileIt->second;
        }

        const auto evidenceIt = evidence_.find(nodeId);
        if (evidenceIt == evidence_.cend()) {
            guess.reasons.append(QStringLiteral("No traffic observed for this node."));
            guesses.push_back(std::move(guess));
            continue;
        }
        const NodeTrafficEvidence& ev = evidenceIt->second;

        const bool snoopTarget = ev.recvSnp || ev.sendsSnpResp || ev.sendsSnpData;

        if (snoopTarget) {
            // Only RNs are snoop targets or source snoop responses.
            if (ev.sendsSnpData || ev.sendsCachedWriteData) {
                guess.type = CLog::NodeType::RN_F;
                guess.confidence = ev.recvSnp
                    ? NodeTypeGuessConfidence::High
                    : NodeTypeGuessConfidence::Medium;
                if (ev.recvSnp) {
                    guess.reasons.append(QStringLiteral("Receives snoop requests."));
                }
                if (ev.sendsSnpData) {
                    guess.reasons.append(QStringLiteral("Sources snoop response data (SnpRespData)."));
                }
                if (ev.sendsCachedWriteData) {
                    guess.reasons.append(QStringLiteral("Issues WriteBack/WriteEvict/WriteClean requests."));
                }
            } else {
                guess.type = CLog::NodeType::RN_D;
                guess.confidence = NodeTypeGuessConfidence::Medium;
                guess.reasons.append(QStringLiteral("Receives snoops but never sources cached data."));
            }
        } else if (ev.sendsSnp) {
            guess.type = CLog::NodeType::HN_F;
            guess.confidence = NodeTypeGuessConfidence::High;
            guess.reasons.append(QStringLiteral("Issues snoop requests (Home-F or MN behavior)."));
        } else if (ev.sendsReq && !ev.recvReq) {
            if (ev.sendsCachedWriteData) {
                guess.type = CLog::NodeType::RN_F;
                guess.confidence = NodeTypeGuessConfidence::Medium;
                guess.reasons.append(QStringLiteral("Issues WriteBack/WriteEvict/WriteClean requests."));
            } else {
                guess.type = CLog::NodeType::RN_I;
                guess.confidence = NodeTypeGuessConfidence::Medium;
                guess.reasons.append(QStringLiteral("Issues requests and is never snooped."));
                if (ev.sendsPrefetchTgt) {
                    guess.reasons.append(QStringLiteral("Issues PrefetchTgt requests (RN-I hint)."));
                }
            }
        } else if (ev.recvReq && !ev.sendsReq) {
            // Home or Subordinate: if a request source is itself home-like,
            // the requests were forwarded downstream toward memory.
            bool fromHome = false;
            for (const quint16 src : ev.incomingReqSources) {
                if (isHomeLike(src)) {
                    fromHome = true;
                    break;
                }
            }
            if (fromHome) {
                guess.type = ev.sendsDat ? CLog::NodeType::SN_F : CLog::NodeType::SN_I;
                guess.confidence = NodeTypeGuessConfidence::Medium;
                guess.reasons.append(QStringLiteral("Receives requests forwarded by a Home node."));
                if (!ev.sendsDat) {
                    guess.reasons.append(QStringLiteral("No data sent; SN-I possible."));
                }
            } else {
                guess.type = CLog::NodeType::HN_I;
                guess.confidence = NodeTypeGuessConfidence::Low;
                guess.reasons.append(
                    QStringLiteral("Receives requests directly and never snoops; HN-F is still "
                                   "possible if the trace contains no sharing."));
            }
        } else if (ev.sendsReq && ev.recvReq) {
            guess.type = CLog::NodeType::HN_F;
            guess.confidence = NodeTypeGuessConfidence::Low;
            guess.reasons.append(QStringLiteral("Issues and receives requests (proxy/DCT behavior)."));
        } else if (ev.sendsRsp || ev.sendsDat) {
            guess.type = CLog::NodeType::SN_F;
            guess.confidence = NodeTypeGuessConfidence::Low;
            guess.reasons.append(QStringLiteral("Only outbound responses or data observed."));
        } else if (ev.recvRsp || ev.recvDat) {
            guess.type = CLog::NodeType::RN_I;
            guess.confidence = NodeTypeGuessConfidence::Low;
            guess.reasons.append(QStringLiteral("Only inbound responses or data observed."));
        } else {
            guess.reasons.append(QStringLiteral("No distinguishing traffic observed."));
        }

        if (guess.type.has_value()
            && ev.sendsReqBeforeSAM
            && !ev.sendsReqAfterSAM) {
            const CLog::NodeType variant = beforeSamVariantOf(*guess.type);
            if (variant != *guess.type) {
                guess.type = variant;
                guess.reasons.append(
                    QStringLiteral("Requests observed only on BeforeSAM channels."));
            }
        }

        guesses.push_back(std::move(guess));
    }

    return guesses;
}

const std::vector<CLog::NodeType>& AllNodeTypes()
{
    static const std::vector<CLog::NodeType> types = {
        CLog::NodeType::RN_F,
        CLog::NodeType::RN_D,
        CLog::NodeType::RN_I,
        CLog::NodeType::HN_F,
        CLog::NodeType::HN_I,
        CLog::NodeType::SN_F,
        CLog::NodeType::SN_I,
        CLog::NodeType::MN,
        CLog::NodeType::RN_F_BeforeSAM,
        CLog::NodeType::RN_D_BeforeSAM,
        CLog::NodeType::RN_I_BeforeSAM,
        CLog::NodeType::HN_F_BeforeSAM,
        CLog::NodeType::HN_I_BeforeSAM,
        CLog::NodeType::SN_F_BeforeSAM,
        CLog::NodeType::SN_I_BeforeSAM,
        CLog::NodeType::MN_BeforeSAM,
    };
    return types;
}

CLog::NodeType BaseNodeType(const CLog::NodeType type) noexcept
{
    switch (type) {
    case CLog::NodeType::RN_F_BeforeSAM:
        return CLog::NodeType::RN_F;
    case CLog::NodeType::RN_D_BeforeSAM:
        return CLog::NodeType::RN_D;
    case CLog::NodeType::RN_I_BeforeSAM:
        return CLog::NodeType::RN_I;
    case CLog::NodeType::HN_F_BeforeSAM:
        return CLog::NodeType::HN_F;
    case CLog::NodeType::HN_I_BeforeSAM:
        return CLog::NodeType::HN_I;
    case CLog::NodeType::SN_F_BeforeSAM:
        return CLog::NodeType::SN_F;
    case CLog::NodeType::SN_I_BeforeSAM:
        return CLog::NodeType::SN_I;
    case CLog::NodeType::MN_BeforeSAM:
        return CLog::NodeType::MN;
    default:
        return type;
    }
}

QString NodeTypeBaseName(const CLog::NodeType type)
{
    return QString::fromStdString(CLog::NodeTypeToString(BaseNodeType(type)));
}

std::optional<CLog::NodeType> NodeTypeFromString(const QString& text)
{
    for (const CLog::NodeType type : AllNodeTypes()) {
        if (text == QString::fromStdString(CLog::NodeTypeToString(type))) {
            return type;
        }
    }
    return std::nullopt;
}

QString NodeTypeGuessConfidenceToString(NodeTypeGuessConfidence confidence)
{
    switch (confidence) {
    case NodeTypeGuessConfidence::High:
        return QStringLiteral("high");
    case NodeTypeGuessConfidence::Medium:
        return QStringLiteral("medium");
    case NodeTypeGuessConfidence::Low:
        return QStringLiteral("low");
    }
    return QString();
}

}  // namespace CHIron::Gui
