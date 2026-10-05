#pragma once

#include "clog_b_trace_loader.hpp"

#include <QStringList>

#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

namespace CHIron::Gui {

enum class NodeTypeGuessConfidence {
    High,
    Medium,
    Low
};

struct NodeTypeGuess {
    quint16 nodeId = 0;
    std::optional<CLog::NodeType> type;             // nullopt: no guess, leave unmapped
    NodeTypeGuessConfidence confidence = NodeTypeGuessConfidence::Low;
    QStringList reasons;
    bool mappedInFile = false;
    std::optional<CLog::NodeType> fileType;
};

struct NodeTrafficEvidence {
    bool sendsReq = false;
    bool recvReq = false;
    bool sendsSnp = false;
    bool recvSnp = false;
    bool sendsRsp = false;
    bool recvRsp = false;
    bool sendsDat = false;
    bool recvDat = false;
    bool sendsSnpResp = false;                      // SnpResp/SnpRespFwded sourced by the node
    bool sendsSnpData = false;                      // SnpRespData* sourced by the node
    bool sendsCachedWriteData = false;              // WriteBack/WriteEvict/WriteClean REQs
    bool sendsPrefetchTgt = false;
    bool sendsReqBeforeSAM = false;                 // REQs sent on *_BeforeSAM channels
    bool sendsReqAfterSAM = false;                  // REQs sent on normal REQ channels
    std::set<quint16> incomingReqSources;
};

// Accumulates per-node traffic evidence from fast records and derives CHI node
// type guesses for observed node IDs, including nodes that are not listed in
// the trace file topology (CHI_TOPOS).
class NodeTypeGuesser {
public:
    void clear() noexcept;
    void observe(const CLogBTraceFastRecordInfo& info);

    std::vector<NodeTypeGuess> results(const CLogBTraceMetadata& metadata) const;

    const std::unordered_map<quint16, NodeTrafficEvidence>& evidence() const noexcept { return evidence_; }

private:
    std::unordered_map<quint16, NodeTrafficEvidence> evidence_;
};

// Lists every CLog::NodeType value in display order (standard types first,
// then the *_BeforeSAM variants).
const std::vector<CLog::NodeType>& AllNodeTypes();

// Strips the *_BeforeSAM variant, returning the base node type (identity for
// base types). BeforeSAM only affects xaction building (SAM scope); node-type
// badges always display the base type tag.
CLog::NodeType BaseNodeType(CLog::NodeType type) noexcept;

// Display name of the base node type ("RN-F" for RN_F_BeforeSAM, etc.).
QString NodeTypeBaseName(CLog::NodeType type);

// Inverse of CLog::NodeTypeToString. Accepts the exact names produced by
// NodeTypeToString (case-sensitive); returns nullopt for anything else.
std::optional<CLog::NodeType> NodeTypeFromString(const QString& text);

QString NodeTypeGuessConfidenceToString(NodeTypeGuessConfidence confidence);

}  // namespace CHIron::Gui
