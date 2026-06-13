#include "address_widget.hpp"
#include "cache_widget.hpp"
#include "clog_b_trace_loader.hpp"
#include "config.hpp"
#include "errors_widget.hpp"
#include "filter_parallel_utils.hpp"
#include "flit_transaction_keys.hpp"
#include "flit_edit_adapter.hpp"
#include "flit_table_model.hpp"
#include "latency_analysis.hpp"
#include "latency_diff_export.hpp"
#include "latency_diff_widget.hpp"
#include "latency_widget.hpp"
#include "main_window.hpp"
#include "marker_widget.hpp"
#include "timeline_widget.hpp"
#include "transaction_widget.hpp"
#include "trace_issue_store.hpp"
#include "trace_statistics.hpp"
#include "trace_session_fast_index_utils.hpp"

#include "chi_eb/xact/chi_eb_joint.hpp"
#include "chi_eb/xact/chi_eb_xact_state.hpp"
#include "chi_eb/util/chi_eb_util_flit.hpp"

#include <QColor>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QHash>
#include <QImage>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QThread>
#include <QStringList>
#include <QTreeWidget>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

using TestFunction = std::function<void()>;

void resetErrorsDispositionSettings()
{
    QSettings settings(QCoreApplication::applicationDirPath() + QStringLiteral("/clogan_gui_layout.ini"),
                       QSettings::IniFormat);
    settings.remove(QStringLiteral("errors"));
    settings.sync();
}

[[noreturn]] void fail(const QString& message)
{
    throw std::runtime_error(message.toStdString());
}

void expect(const bool condition, const QString& message)
{
    if (!condition) {
        fail(message);
    }
}

void expectEqual(const int actual, const int expected, const QString& message)
{
    if (actual != expected) {
        fail(QStringLiteral("%1 (actual=%2, expected=%3)")
                 .arg(message)
                 .arg(actual)
                 .arg(expected));
    }
}

void expectNear(const int actual, const int expected, const int tolerance, const QString& message)
{
    if (std::abs(actual - expected) > tolerance) {
        fail(QStringLiteral("%1 (actual=%2, expected=%3, tolerance=%4)")
                 .arg(message)
                 .arg(actual)
                 .arg(expected)
                 .arg(tolerance));
    }
}

void expectNear(const double actual, const double expected, const double tolerance, const QString& message)
{
    if (std::abs(actual - expected) > tolerance) {
        fail(QStringLiteral("%1 (actual=%2, expected=%3, tolerance=%4)")
                 .arg(message)
                 .arg(actual)
                 .arg(expected)
                 .arg(tolerance));
    }
}

void expectGreater(const double actual, const double expected, const QString& message)
{
    if (!(actual > expected)) {
        fail(QStringLiteral("%1 (actual=%2, expected>%3)")
                 .arg(message)
                 .arg(actual)
                 .arg(expected));
    }
}

void expectEqual(const std::size_t actual, const std::size_t expected, const QString& message)
{
    if (actual != expected) {
        fail(QStringLiteral("%1 (actual=%2, expected=%3)")
                 .arg(message)
                 .arg(static_cast<qulonglong>(actual))
                 .arg(static_cast<qulonglong>(expected)));
    }
}

void expectEqual(const QString& actual, const QString& expected, const QString& message)
{
    if (actual != expected) {
        fail(QStringLiteral("%1 (actual=%2, expected=%3)")
                 .arg(message, actual, expected));
    }
}

void expectVectorEqual(const std::vector<std::size_t>& actual,
                       const std::vector<std::size_t>& expected,
                       const QString& message)
{
    if (actual == expected) {
        return;
    }

    fail(QStringLiteral("%1 (actual-size=%2, expected-size=%3)")
             .arg(message)
             .arg(static_cast<qulonglong>(actual.size()))
             .arg(static_cast<qulonglong>(expected.size())));
}

std::uint8_t storedByteLengthForTest(const std::size_t bitWidth)
{
    return static_cast<std::uint8_t>((bitWidth + 7U) / 8U);
}

CLog::CLogB::TagCHIRecords::Record makeLengthOnlyRecord(const CLog::Channel channel,
                                                        const std::uint8_t byteLength)
{
    CLog::CLogB::TagCHIRecords::Record record;
    record.timeShift = 1;
    record.nodeId = 1;
    record.channel = channel;
    record.flitLength = byteLength;
    record.flit = std::shared_ptr<std::uint32_t[]>(new std::uint32_t[(byteLength + 3U) / 4U]{});
    return record;
}

class TestFlitBitWriter final {
public:
    explicit TestFlitBitWriter(std::uint32_t* flitBits) noexcept
        : flitBits_(flitBits)
    {
    }

    void append(std::uint64_t value, std::size_t width) noexcept
    {
        std::size_t consumed = 0;
        while (width > 0) {
            const std::size_t wordIndex = bitOffset_ / 32U;
            const std::size_t wordOffset = bitOffset_ % 32U;
            const std::size_t chunkWidth = std::min<std::size_t>(width, 32U - wordOffset);
            const std::uint64_t mask = (std::uint64_t{1} << chunkWidth) - 1U;
            flitBits_[wordIndex] |= static_cast<std::uint32_t>(((value >> consumed) & mask) << wordOffset);
            bitOffset_ += chunkWidth;
            consumed += chunkWidth;
            width -= chunkWidth;
        }
    }

    void finish(const std::size_t expectedBitOffset) const
    {
        expect(bitOffset_ == expectedBitOffset,
               QStringLiteral("Test flit bit writer produced an unexpected bit width."));
    }

private:
    std::uint32_t* flitBits_ = nullptr;
    std::size_t bitOffset_ = 0;
};

template<typename ValueT>
void appendField(TestFlitBitWriter& writer, const ValueT& value, const std::size_t width)
{
    writer.append(static_cast<std::uint64_t>(value), width);
}

template<typename ReqFlit>
void appendEbReqFlitForTest(const ReqFlit& flit, TestFlitBitWriter& writer)
{
    appendField(writer, flit.QoS(), ReqFlit::QOS_WIDTH);
    appendField(writer, flit.TgtID(), ReqFlit::TGTID_WIDTH);
    appendField(writer, flit.SrcID(), ReqFlit::SRCID_WIDTH);
    appendField(writer, flit.TxnID(), ReqFlit::TXNID_WIDTH);
    appendField(writer, flit.ReturnNID(), ReqFlit::RETURNNID_WIDTH);
    appendField(writer, flit.StashNIDValid(), ReqFlit::STASHNIDVALID_WIDTH);
    appendField(writer, flit.ReturnTxnID(), ReqFlit::RETURNTXNID_WIDTH);
    appendField(writer, flit.Opcode(), ReqFlit::OPCODE_WIDTH);
    appendField(writer, flit.Size(), ReqFlit::SSIZE_WIDTH);

    const std::uint64_t address = static_cast<std::uint64_t>(flit.Addr());
    writer.append(address, 32);
    writer.append(address >> 32U, ReqFlit::ADDR_WIDTH - 32U);

    appendField(writer, flit.NS(), ReqFlit::NS_WIDTH);
    appendField(writer, flit.LikelyShared(), ReqFlit::LIKELYSHARED_WIDTH);
    appendField(writer, flit.AllowRetry(), ReqFlit::ALLOWRETRY_WIDTH);
    appendField(writer, flit.Order(), ReqFlit::ORDER_WIDTH);
    appendField(writer, flit.PCrdType(), ReqFlit::PCRDTYPE_WIDTH);
    appendField(writer, flit.MemAttr(), ReqFlit::MEMATTR_WIDTH);
    appendField(writer, flit.SnpAttr(), ReqFlit::SNPATTR_WIDTH);
    appendField(writer, flit.TagGroupID(), ReqFlit::TAGGROUPID_WIDTH);
    appendField(writer, flit.Excl(), ReqFlit::EXCL_WIDTH);
    appendField(writer, flit.ExpCompAck(), ReqFlit::EXPCOMPACK_WIDTH);
    appendField(writer, flit.TagOp(), ReqFlit::TAGOP_WIDTH);
    appendField(writer, flit.TraceTag(), ReqFlit::TRACETAG_WIDTH);
    if constexpr (ReqFlit::hasMPAM) {
        appendField(writer, flit.MPAM(), ReqFlit::MPAM_WIDTH);
    }
    if constexpr (ReqFlit::hasRSVDC) {
        appendField(writer, flit.RSVDC(), ReqFlit::RSVDC_WIDTH);
    }
}

template<typename DatFlit>
void appendEbDatFlitForTest(const DatFlit& flit, TestFlitBitWriter& writer)
{
    appendField(writer, flit.QoS(), DatFlit::QOS_WIDTH);
    appendField(writer, flit.TgtID(), DatFlit::TGTID_WIDTH);
    appendField(writer, flit.SrcID(), DatFlit::SRCID_WIDTH);
    appendField(writer, flit.TxnID(), DatFlit::TXNID_WIDTH);
    appendField(writer, flit.HomeNID(), DatFlit::HOMENID_WIDTH);
    appendField(writer, flit.Opcode(), DatFlit::OPCODE_WIDTH);
    appendField(writer, flit.RespErr(), DatFlit::RESPERR_WIDTH);
    appendField(writer, flit.Resp(), DatFlit::RESP_WIDTH);
    appendField(writer, flit.DataSource(), DatFlit::DATASOURCE_WIDTH);
    appendField(writer, flit.CBusy(), DatFlit::CBUSY_WIDTH);
    appendField(writer, flit.DBID(), DatFlit::DBID_WIDTH);
    appendField(writer, flit.CCID(), DatFlit::CCID_WIDTH);
    appendField(writer, flit.DataID(), DatFlit::DATAID_WIDTH);
    appendField(writer, flit.TagOp(), DatFlit::TAGOP_WIDTH);
    appendField(writer, flit.Tag(), DatFlit::TAG_WIDTH);
    appendField(writer, flit.TU(), DatFlit::TU_WIDTH);
    appendField(writer, flit.TraceTag(), DatFlit::TRACETAG_WIDTH);
    if constexpr (DatFlit::hasRSVDC) {
        appendField(writer, flit.RSVDC(), DatFlit::RSVDC_WIDTH);
    }

    const std::uint64_t byteEnable = static_cast<std::uint64_t>(flit.BE());
    if constexpr (DatFlit::BE_WIDTH > 32) {
        writer.append(byteEnable, 32);
        writer.append(byteEnable >> 32U, DatFlit::BE_WIDTH - 32U);
    } else {
        writer.append(byteEnable, DatFlit::BE_WIDTH);
    }

    for (std::size_t index = 0; index < DatFlit::DATA_WIDTH / 64U; ++index) {
        const std::uint64_t dataWord = static_cast<std::uint64_t>(flit.Data()[index]);
        writer.append(dataWord, 32);
        writer.append(dataWord >> 32U, 32);
    }

    if constexpr (DatFlit::hasDataCheck) {
        const std::uint64_t dataCheck = static_cast<std::uint64_t>(flit.DataCheck());
        if constexpr (DatFlit::DATACHECK_WIDTH > 32) {
            writer.append(dataCheck, 32);
            writer.append(dataCheck >> 32U, DatFlit::DATACHECK_WIDTH - 32U);
        } else {
            writer.append(dataCheck, DatFlit::DATACHECK_WIDTH);
        }
    }
    if constexpr (DatFlit::hasPoison) {
        appendField(writer, flit.Poison(), DatFlit::POISON_WIDTH);
    }
}

template<typename RspFlit>
void appendEbRspFlitForTest(const RspFlit& flit, TestFlitBitWriter& writer)
{
    appendField(writer, flit.QoS(), RspFlit::QOS_WIDTH);
    appendField(writer, flit.TgtID(), RspFlit::TGTID_WIDTH);
    appendField(writer, flit.SrcID(), RspFlit::SRCID_WIDTH);
    appendField(writer, flit.TxnID(), RspFlit::TXNID_WIDTH);
    appendField(writer, flit.Opcode(), RspFlit::OPCODE_WIDTH);
    appendField(writer, flit.RespErr(), RspFlit::RESPERR_WIDTH);
    appendField(writer, flit.Resp(), RspFlit::RESP_WIDTH);
    appendField(writer, flit.FwdState(), RspFlit::FWDSTATE_WIDTH);
    appendField(writer, flit.CBusy(), RspFlit::CBUSY_WIDTH);
    appendField(writer, flit.DBID(), RspFlit::DBID_WIDTH);
    appendField(writer, flit.PCrdType(), RspFlit::PCRDTYPE_WIDTH);
    appendField(writer, flit.TagOp(), RspFlit::TAGOP_WIDTH);
    appendField(writer, flit.TraceTag(), RspFlit::TRACETAG_WIDTH);
}

template<typename SnpFlit>
void appendEbSnpFlitForTest(const SnpFlit& flit, TestFlitBitWriter& writer)
{
    appendField(writer, flit.QoS(), SnpFlit::QOS_WIDTH);
    appendField(writer, flit.SrcID(), SnpFlit::SRCID_WIDTH);
    appendField(writer, flit.TxnID(), SnpFlit::TXNID_WIDTH);
    appendField(writer, flit.FwdNID(), SnpFlit::FWDNID_WIDTH);
    appendField(writer, flit.FwdTxnID(), SnpFlit::FWDTXNID_WIDTH);
    appendField(writer, flit.Opcode(), SnpFlit::OPCODE_WIDTH);

    const std::uint64_t address = static_cast<std::uint64_t>(flit.Addr());
    writer.append(address, 32);
    writer.append(address >> 32U, SnpFlit::ADDR_WIDTH - 32U);

    appendField(writer, flit.NS(), SnpFlit::NS_WIDTH);
    appendField(writer, flit.DoNotGoToSD(), SnpFlit::DONOTGOTOSD_WIDTH);
    appendField(writer, flit.RetToSrc(), SnpFlit::RETTOSRC_WIDTH);
    appendField(writer, flit.TraceTag(), SnpFlit::TRACETAG_WIDTH);
    if constexpr (SnpFlit::hasMPAM) {
        appendField(writer, flit.MPAM(), SnpFlit::MPAM_WIDTH);
    }
}

template<typename FlitT, typename SerializerT>
CLog::CLogB::TagCHIRecords::Record makeSerializedRecord(const CLog::Channel channel,
                                                        const std::uint16_t nodeId,
                                                        const std::uint32_t timeShift,
                                                        const FlitT& flit,
                                                        SerializerT serializer)
{
    const std::uint8_t byteLength = storedByteLengthForTest(FlitT::WIDTH);
    CLog::CLogB::TagCHIRecords::Record record;
    record.timeShift = timeShift;
    record.nodeId = nodeId;
    record.channel = channel;
    record.flitLength = byteLength;
    record.flit = std::shared_ptr<std::uint32_t[]>(new std::uint32_t[(byteLength + 3U) / 4U]{});
    TestFlitBitWriter writer(record.flit.get());
    serializer(flit, writer);
    writer.finish(FlitT::WIDTH);
    return record;
}

void writeLengthOnlyTrace(const QString& filePath,
                          const CLog::Parameters& declaredParameters,
                          std::vector<CLog::CLogB::TagCHIRecords::Record> records)
{
    std::vector<std::vector<CLog::CLogB::TagCHIRecords::Record>> blocks;
    blocks.push_back(std::move(records));

    std::ofstream output(std::filesystem::path(filePath.toStdWString()), std::ios::binary);
    if (!output) {
        fail(QStringLiteral("Temporary CLog.B file creation failed."));
    }

    CLog::CLogB::TagCHIParameters parameterTag;
    parameterTag.parameters = declaredParameters;
    CLog::CLogB::Writer().Next(output, &parameterTag);

    for (auto& blockRecords : blocks) {
        CLog::CLogB::TagCHIRecords recordTag;
        recordTag.head.timeBase = 0;
        recordTag.records = std::move(blockRecords);
        CLog::CLogB::Writer().Next(output, &recordTag);
    }
}

void writeLengthOnlyTraceBlocks(const QString& filePath,
                                const CLog::Parameters& declaredParameters,
                                std::vector<std::vector<CLog::CLogB::TagCHIRecords::Record>> blocks)
{
    std::ofstream output(std::filesystem::path(filePath.toStdWString()), std::ios::binary);
    if (!output) {
        fail(QStringLiteral("Temporary CLog.B file creation failed."));
    }

    CLog::CLogB::TagCHIParameters parameterTag;
    parameterTag.parameters = declaredParameters;
    CLog::CLogB::Writer().Next(output, &parameterTag);

    std::uint64_t timeBase = 0;
    for (auto& blockRecords : blocks) {
        CLog::CLogB::TagCHIRecords recordTag;
        recordTag.head.timeBase = timeBase;
        recordTag.records = std::move(blockRecords);
        CLog::CLogB::Writer().Next(output, &recordTag);
        timeBase += 1000;
    }
}

void writeTraceWithTopology(const QString& filePath,
                            const CLog::Parameters& declaredParameters,
                            std::vector<CLog::CLogB::TagCHITopologies::Node> topologyNodes,
                            std::vector<CLog::CLogB::TagCHIRecords::Record> records)
{
    std::ofstream output(std::filesystem::path(filePath.toStdWString()), std::ios::binary);
    if (!output) {
        fail(QStringLiteral("Temporary CLog.B file creation failed."));
    }

    CLog::CLogB::TagCHIParameters parameterTag;
    parameterTag.parameters = declaredParameters;
    CLog::CLogB::Writer().Next(output, &parameterTag);

    CLog::CLogB::TagCHITopologies topologyTag;
    topologyTag.nodes = std::move(topologyNodes);
    CLog::CLogB::Writer().Next(output, &topologyTag);

    CLog::CLogB::TagCHIRecords recordTag;
    recordTag.head.timeBase = 0;
    recordTag.records = std::move(records);
    CLog::CLogB::Writer().Next(output, &recordTag);
}

bool sameParameters(const CLog::Parameters& lhs, const CLog::Parameters& rhs)
{
    return lhs.GetIssue() == rhs.GetIssue()
        && lhs.GetNodeIdWidth() == rhs.GetNodeIdWidth()
        && lhs.GetReqAddrWidth() == rhs.GetReqAddrWidth()
        && lhs.GetReqRSVDCWidth() == rhs.GetReqRSVDCWidth()
        && lhs.GetDatRSVDCWidth() == rhs.GetDatRSVDCWidth()
        && lhs.GetDataWidth() == rhs.GetDataWidth()
        && lhs.IsDataCheckPresent() == rhs.IsDataCheckPresent()
        && lhs.IsPoisonPresent() == rhs.IsPoisonPresent()
        && lhs.IsMPAMPresent() == rhs.IsMPAMPresent();
}

QString firstXactionTransactionKey(const CHIron::Gui::FlitRecord& record)
{
    for (const QString& key : CHIron::Gui::TransactionGroupKeys(record)) {
        if (key.startsWith(QStringLiteral("xaction|"))) {
            return key;
        }
    }
    return QString();
}

std::vector<CHIron::Gui::FlitRecord> buildFilterTestRows()
{
    using CHIron::Gui::FlitChannel;
    using CHIron::Gui::FlitDirection;
    using CHIron::Gui::FlitRecord;

    auto makeRecord = [](const qint64 timestamp,
                         const FlitChannel channel,
                         const FlitDirection direction,
                         const QString& opcode,
                         const QString& opcodeRaw,
                         const QString& source,
                         const QString& target,
                         const QString& txnId,
                         const QString& address,
                         const QString& dbid,
                         const QString& dataId,
                         const QString& summary) {
        FlitRecord record;
        record.timestamp = timestamp;
        record.channel = channel;
        record.direction = direction;
        record.opcode = opcode;
        record.opcodeRaw = opcodeRaw;
        record.source = source;
        record.target = target;
        record.txnId = txnId;
        record.address = address;
        record.dbid = dbid;
        record.dataId = dataId;
        record.summary = summary;
        return record;
    };

    return {
        makeRecord(100,
                   FlitChannel::Req,
                   FlitDirection::Tx,
                   QStringLiteral("ReadNotSharedDirty"),
                   QStringLiteral("0x1"),
                   QStringLiteral("40"),
                   QStringLiteral("10"),
                   QStringLiteral("4"),
                   QStringLiteral("0x0000000000012000"),
                   QString(),
                   QString(),
                   QStringLiteral("REQ address row")),
        makeRecord(112,
                   FlitChannel::Rsp,
                   FlitDirection::Rx,
                   QStringLiteral("Comp"),
                   QStringLiteral("0x4"),
                   QStringLiteral("10"),
                   QStringLiteral("40"),
                   QStringLiteral("4"),
                   QString(),
                   QStringLiteral("12"),
                   QString(),
                   QStringLiteral("RSP DBID row")),
        makeRecord(118,
                   FlitChannel::Dat,
                   FlitDirection::Rx,
                   QStringLiteral("CompData"),
                   QStringLiteral("0x8"),
                   QStringLiteral("10"),
                   QStringLiteral("40"),
                   QStringLiteral("4"),
                   QString(),
                   QStringLiteral("12"),
                   QStringLiteral("0"),
                   QStringLiteral("DAT DBID row 0")),
        makeRecord(124,
                   FlitChannel::Dat,
                   FlitDirection::Rx,
                   QStringLiteral("CompData"),
                   QStringLiteral("0x8"),
                   QStringLiteral("10"),
                   QStringLiteral("40"),
                   QStringLiteral("4"),
                   QString(),
                   QStringLiteral("12"),
                   QStringLiteral("2"),
                   QStringLiteral("DAT DBID row 2")),
    };
}

std::vector<CHIron::Gui::FlitRecord> buildOptionalFieldStateRows(const int rowCount = 4)
{
    const std::vector<CHIron::Gui::FlitRecord> baseRows = buildFilterTestRows();
    const std::array<QString, 4> values = {
        QStringLiteral("7"),
        QStringLiteral("3"),
        QStringLiteral("1"),
        QStringLiteral("5"),
    };
    std::vector<CHIron::Gui::FlitRecord> rows;
    rows.reserve(static_cast<std::size_t>(qMax(0, rowCount)));
    for (int index = 0; index < rowCount; ++index) {
        CHIron::Gui::FlitRecord row = baseRows[static_cast<std::size_t>(index) % baseRows.size()];
        row.timestamp += index * 10;
        row.summary = QStringLiteral("%1 copy %2").arg(row.summary).arg(index);
        CHIron::Gui::FieldEntry field;
        field.scope = CHIron::Gui::InternFieldText(CHIron::Gui::ToString(row.channel));
        field.name = CHIron::Gui::InternFieldText(QStringLiteral("QoS"));
        field.value = values[static_cast<std::size_t>(index) % values.size()];
        field.raw = QStringLiteral("0x%1").arg(field.value.toInt(), 0, 16).toUpper();
        row.fields.push_back(std::move(field));
        rows.push_back(std::move(row));
    }
    return rows;
}

CLog::Parameters makeDefaultTestParameters()
{
    CLog::Parameters parameters;
    parameters.SetIssue(CLog::Issue::Eb);
    expect(parameters.SetNodeIdWidth(CHIron::Gui::ViewerConfig::nodeIdWidth),
           QStringLiteral("Failed to set test NodeIDWidth."));
    expect(parameters.SetReqAddrWidth(CHIron::Gui::ViewerConfig::reqAddrWidth),
           QStringLiteral("Failed to set test ReqAddrWidth."));
    expect(parameters.SetReqRSVDCWidth(CHIron::Gui::ViewerConfig::reqRsvdcWidth),
           QStringLiteral("Failed to set test ReqRSVDCWidth."));
    expect(parameters.SetDatRSVDCWidth(CHIron::Gui::ViewerConfig::datRsvdcWidth),
           QStringLiteral("Failed to set test DatRSVDCWidth."));
    expect(parameters.SetDataWidth(CHIron::Gui::ViewerConfig::dataWidth),
           QStringLiteral("Failed to set test DataWidth."));
    parameters.SetDataCheckPresent(CHIron::Gui::ViewerConfig::dataCheckWidth != 0);
    parameters.SetPoisonPresent(CHIron::Gui::ViewerConfig::poisonWidth != 0);
    parameters.SetMPAMPresent(CHIron::Gui::ViewerConfig::mpamWidth != 0);
    return parameters;
}

std::vector<CHIron::Gui::FlitRecord> buildLatencyWidgetTestRows();

std::vector<CHIron::Gui::FlitRecord> buildStatisticsStressRows(const int rowCount)
{
    using CHIron::Gui::FlitChannel;
    using CHIron::Gui::FlitDirection;
    using CHIron::Gui::FlitRecord;

    std::vector<FlitRecord> rows;
    rows.reserve(static_cast<std::size_t>(rowCount));
    for (int index = 0; index < rowCount; ++index) {
        FlitRecord record;
        record.timestamp = 1000 + index;
        record.channel = (index % 4 == 0) ? FlitChannel::Req
            : (index % 4 == 1) ? FlitChannel::Rsp
            : (index % 4 == 2) ? FlitChannel::Dat
                               : FlitChannel::Snp;
        record.direction = (index % 2 == 0) ? FlitDirection::Tx : FlitDirection::Rx;
        record.opcode = QStringLiteral("StatOp%1").arg(index % 17);
        record.opcodeRaw = QStringLiteral("0x%1").arg(index % 17, 0, 16);
        record.source = QString::number(index % 32);
        record.target = QString::number((index + 5) % 32);
        if (record.channel == FlitChannel::Req || record.channel == FlitChannel::Snp) {
            record.address = QStringLiteral("0x%1").arg(0x1000 + index * 64, 0, 16);
        }
        if (record.channel == FlitChannel::Rsp || record.channel == FlitChannel::Dat) {
            record.dbid = QString::number(index % 64);
        }
        record.summary = QStringLiteral("Statistics stress row %1").arg(index);
        rows.push_back(std::move(record));
    }
    return rows;
}

bool waitForCondition(const std::function<bool()>& condition, const int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs) {
        QApplication::processEvents();
        QThread::msleep(2);
    }
    QApplication::processEvents();
    return condition();
}

std::vector<CHIron::Gui::FlitRecord> buildTimelineTestRows()
{
    using CHIron::Gui::FlitChannel;
    using CHIron::Gui::FlitDirection;
    using CHIron::Gui::FlitRecord;

    std::vector<FlitRecord> rows;
    rows.reserve(256);
    for (int index = 0; index < 256; ++index) {
        FlitRecord record;
        record.timestamp = 1000 + index * 10;
        record.channel = (index % 4 == 0) ? FlitChannel::Req
            : (index % 4 == 1) ? FlitChannel::Rsp
            : (index % 4 == 2) ? FlitChannel::Dat
                               : FlitChannel::Snp;
        record.direction = (index % 2 == 0) ? FlitDirection::Tx : FlitDirection::Rx;
        record.channelNodeId = static_cast<quint16>(index % 6);
        record.opcode = QStringLiteral("TimelineOp");
        record.summary = QStringLiteral("Timeline row %1").arg(index);
        rows.push_back(std::move(record));
    }
    return rows;
}

std::vector<CHIron::Gui::FlitRecord> buildSparseTimelineTestRows()
{
    using CHIron::Gui::FlitChannel;
    using CHIron::Gui::FlitDirection;
    using CHIron::Gui::FlitRecord;

    const std::array<qint64, 4> timestamps = {0, 10, 20, 1000};
    std::vector<FlitRecord> rows;
    rows.reserve(timestamps.size());
    for (std::size_t index = 0; index < timestamps.size(); ++index) {
        FlitRecord record;
        record.timestamp = timestamps[index];
        record.channel = FlitChannel::Req;
        record.direction = FlitDirection::Tx;
        record.channelNodeId = 0;
        record.opcode = QStringLiteral("SparseTimelineOp");
        record.summary = QStringLiteral("Sparse timeline row %1").arg(index);
        rows.push_back(std::move(record));
    }
    return rows;
}

std::vector<CHIron::Gui::FlitRecord> buildShiftedTimelineTestRows()
{
    std::vector<CHIron::Gui::FlitRecord> rows = buildTimelineTestRows();
    for (CHIron::Gui::FlitRecord& row : rows) {
        row.timestamp += 100000;
    }
    return rows;
}

std::vector<CHIron::Gui::FlitRecord> buildSecondSessionSwitchRows()
{
    std::vector<CHIron::Gui::FlitRecord> rows = buildTimelineTestRows();
    for (std::size_t index = 0; index < rows.size(); ++index) {
        CHIron::Gui::FlitRecord& row = rows[index];
        row.timestamp += 500000;
        row.channelNodeId = static_cast<quint16>((index + 3U) % 9U);
        row.opcode = QStringLiteral("SecondSwitchTimelineOp%1").arg(index % 5);
        if (index == 8 || index == 64 || index == 192) {
            row.channel = CHIron::Gui::FlitChannel::Req;
            row.opcode = QStringLiteral("ReadNoSnp");
            row.address = QStringLiteral("0x%1")
                              .arg(QString::number(static_cast<qulonglong>(0x200000 + index * 64),
                                                   16)
                                       .toUpper()
                                       .rightJustified(12, QLatin1Char('0')));
        } else if (index == 40 || index == 160) {
            row.channel = CHIron::Gui::FlitChannel::Snp;
            row.opcode = QStringLiteral("SnpOnce");
            row.address = QStringLiteral("0x%1")
                              .arg(QString::number(static_cast<qulonglong>(0x400000 + index * 64),
                                                   16)
                                       .toUpper()
                                       .rightJustified(12, QLatin1Char('0')));
        } else {
            row.address.clear();
        }
    }
    return rows;
}

std::vector<CHIron::Gui::FlitRecord> buildDenseTimelineOpcodeRows()
{
    using CHIron::Gui::FlitChannel;
    using CHIron::Gui::FlitDirection;
    using CHIron::Gui::FlitRecord;

    std::vector<FlitRecord> rows;
    rows.reserve(64);
    for (int index = 0; index < 64; ++index) {
        FlitRecord record;
        record.timestamp = 2000 + index;
        record.channel = FlitChannel::Req;
        record.direction = FlitDirection::Tx;
        record.channelNodeId = 0;
        record.opcode = QStringLiteral("DenseOpcode");
        record.summary = QStringLiteral("Dense timeline row %1").arg(index);
        rows.push_back(std::move(record));
    }
    return rows;
}

std::vector<CHIron::Gui::FlitRecord> buildAddressWidgetTestRows()
{
    using CHIron::Gui::FlitChannel;
    using CHIron::Gui::FlitDirection;
    using CHIron::Gui::FlitRecord;

    std::vector<FlitRecord> rows;
    rows.reserve(12);
    for (int index = 0; index < 12; ++index) {
        FlitRecord record;
        record.timestamp = 5000 + index * 5;
        record.direction = (index % 2 == 0) ? FlitDirection::Tx : FlitDirection::Rx;
        record.channel = FlitChannel::Dat;
        record.opcode = QStringLiteral("CompData");
        if (index == 1) {
            record.channel = FlitChannel::Req;
            record.opcode = QStringLiteral("ReadNoSnp");
            record.address = QStringLiteral("0x000000001000");
        } else if (index == 4) {
            record.channel = FlitChannel::Req;
            record.opcode = QStringLiteral("WriteBackFull");
            record.address = QStringLiteral("0x000000004000");
        } else if (index == 7) {
            record.channel = FlitChannel::Snp;
            record.opcode = QStringLiteral("SnpOnce");
            record.address = QStringLiteral("0x000000002000");
        } else if (index == 10) {
            record.channel = FlitChannel::Req;
            record.opcode = QStringLiteral("Evict");
            record.address = QStringLiteral("0x000000008000");
        }
        rows.push_back(std::move(record));
    }
    return rows;
}

std::vector<CHIron::Gui::FlitRecord> buildSparseAddressWidgetTestRows()
{
    using CHIron::Gui::FlitChannel;
    using CHIron::Gui::FlitDirection;
    using CHIron::Gui::FlitRecord;

    const std::array<qint64, 3> timestamps = {0, 10, 1000};
    std::vector<FlitRecord> rows;
    rows.reserve(timestamps.size());
    for (std::size_t index = 0; index < timestamps.size(); ++index) {
        FlitRecord record;
        record.timestamp = timestamps[index];
        record.direction = FlitDirection::Tx;
        record.channel = index == 2 ? FlitChannel::Snp : FlitChannel::Req;
        record.opcode = index == 1 ? QStringLiteral("ReadNoSnp") : QStringLiteral("SnpOnce");
        record.address = QStringLiteral("0x%1")
                             .arg(QString::number(static_cast<qulonglong>(0x1000 + index * 0x1000),
                                                  16)
                                      .toUpper()
                                      .rightJustified(12, QLatin1Char('0')));
        rows.push_back(std::move(record));
    }
    return rows;
}

std::vector<CHIron::Gui::FlitRecord> buildDenseAddressWidgetTestRows()
{
    using CHIron::Gui::FlitChannel;
    using CHIron::Gui::FlitDirection;
    using CHIron::Gui::FlitRecord;

    std::vector<FlitRecord> rows;
    rows.reserve(25000);
    for (int index = 0; index < 25000; ++index) {
        FlitRecord record;
        record.timestamp = 7000 + index;
        record.direction = (index % 2 == 0) ? FlitDirection::Tx : FlitDirection::Rx;
        record.channel = (index % 11 == 0) ? FlitChannel::Snp : FlitChannel::Req;
        record.opcode = record.channel == FlitChannel::Snp
            ? QStringLiteral("SnpOnce")
            : (index % 3 == 0 ? QStringLiteral("ReadNoSnp") : QStringLiteral("WriteBackFull"));
        record.address = QStringLiteral("0x%1")
                             .arg(QString::number(static_cast<qulonglong>(0x1000 + (index % 4096) * 64),
                                                  16)
                                      .toUpper()
                                      .rightJustified(12, QLatin1Char('0')));
        rows.push_back(std::move(record));
    }
    return rows;
}

void showTimelineForTest(CHIron::Gui::TimelineWidget& widget)
{
    widget.resize(820, 260);
    widget.ensurePolished();
    widget.buildView();
    QApplication::processEvents();
}

void sendTimelineWheel(CHIron::Gui::TimelineWidget& widget,
                       const QPoint& position,
                       const int deltaY,
                       const Qt::KeyboardModifiers modifiers)
{
    QWheelEvent event(QPointF(position),
                      QPointF(widget.mapToGlobal(position)),
                      QPoint(),
                      QPoint(0, deltaY),
                      Qt::NoButton,
                      modifiers,
                      Qt::NoScrollPhase,
                      false);
    QApplication::sendEvent(&widget, &event);
    QApplication::processEvents();
}

void sendTimelineMouse(CHIron::Gui::TimelineWidget& widget,
                       const QEvent::Type type,
                       const QPoint& position,
                       const Qt::MouseButton button,
                       const Qt::MouseButtons buttons,
                       const Qt::KeyboardModifiers modifiers)
{
    QMouseEvent event(type,
                      QPointF(position),
                      QPointF(widget.mapToGlobal(position)),
                      button,
                      buttons,
                      modifiers);
    QApplication::sendEvent(&widget, &event);
    QApplication::processEvents();
}

void sendTimelineKey(CHIron::Gui::TimelineWidget& widget,
                     const int key,
                     const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QApplication::sendEvent(&widget, &event);
    QApplication::processEvents();
}

void showAddressForTest(CHIron::Gui::AddressWidget& widget)
{
    widget.resize(840, 300);
    widget.ensurePolished();
    widget.buildView();
    QApplication::processEvents();
}

void sendAddressWheel(CHIron::Gui::AddressWidget& widget,
                      const QPoint& position,
                      const int deltaY,
                      const Qt::KeyboardModifiers modifiers)
{
    QWheelEvent event(QPointF(position),
                      QPointF(widget.mapToGlobal(position)),
                      QPoint(),
                      QPoint(0, deltaY),
                      Qt::NoButton,
                      modifiers,
                      Qt::NoScrollPhase,
                      false);
    QApplication::sendEvent(&widget, &event);
    QApplication::processEvents();
}

void sendAddressMouse(CHIron::Gui::AddressWidget& widget,
                      const QEvent::Type type,
                      const QPoint& position,
                      const Qt::MouseButton button,
                      const Qt::MouseButtons buttons,
                      const Qt::KeyboardModifiers modifiers)
{
    QMouseEvent event(type,
                      QPointF(position),
                      QPointF(widget.mapToGlobal(position)),
                      button,
                      buttons,
                      modifiers);
    QApplication::sendEvent(&widget, &event);
    QApplication::processEvents();
}

void sendAddressKey(CHIron::Gui::AddressWidget& widget,
                    const int key,
                    const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QApplication::sendEvent(&widget, &event);
    QApplication::processEvents();
}

void sendTransactionMouse(CHIron::Gui::TransactionWidget& widget,
                          const QEvent::Type type,
                          const QPoint& position,
                          const Qt::MouseButton button,
                          const Qt::MouseButtons buttons,
                          const Qt::KeyboardModifiers modifiers)
{
    QMouseEvent event(type,
                      QPointF(position),
                      QPointF(widget.mapToGlobal(position)),
                      button,
                      buttons,
                      modifiers);
    QApplication::sendEvent(&widget, &event);
    QApplication::processEvents();
}

void sendTransactionWheel(CHIron::Gui::TransactionWidget& widget,
                          const QPoint& position,
                          const int deltaY,
                          const Qt::KeyboardModifiers modifiers)
{
    QWheelEvent event(QPointF(position),
                      QPointF(widget.mapToGlobal(position)),
                      QPoint(),
                      QPoint(0, deltaY),
                      Qt::NoButton,
                      modifiers,
                      Qt::NoScrollPhase,
                      false);
    QApplication::sendEvent(&widget, &event);
    QApplication::processEvents();
}

void sendTransactionKey(CHIron::Gui::TransactionWidget& widget,
                        const int key,
                        const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QApplication::sendEvent(&widget, &event);
    QApplication::processEvents();
}

void showCacheForTest(CHIron::Gui::CacheWidget& widget)
{
    widget.resize(900, 360);
    widget.ensurePolished();
    widget.show();
    QApplication::processEvents();
}

void sendCacheMouse(CHIron::Gui::CacheWidget& widget,
                    const QEvent::Type type,
                    const QPoint& position,
                    const Qt::MouseButton button,
                    const Qt::MouseButtons buttons,
                    const Qt::KeyboardModifiers modifiers)
{
    QMouseEvent event(type,
                      QPointF(position),
                      QPointF(widget.mapToGlobal(position)),
                      button,
                      buttons,
                      modifiers);
    QApplication::sendEvent(&widget, &event);
    QApplication::processEvents();
}

void sendCacheKey(CHIron::Gui::CacheWidget& widget,
                  const int key,
                  const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QApplication::sendEvent(&widget, &event);
    QApplication::processEvents();
}

QImage renderCacheImage(const CHIron::Gui::CacheWidget& widget)
{
    QImage image(widget.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    const_cast<CHIron::Gui::CacheWidget&>(widget).render(&painter);
    painter.end();
    return image;
}

void typeTextIntoLineEdit(QLineEdit& edit, const QString& text)
{
    edit.setFocus();
    edit.clear();
    for (const QChar ch : text) {
        QKeyEvent press(QEvent::KeyPress, 0, Qt::NoModifier, QString(ch));
        QApplication::sendEvent(&edit, &press);
        QKeyEvent release(QEvent::KeyRelease, 0, Qt::NoModifier, QString(ch));
        QApplication::sendEvent(&edit, &release);
        QApplication::processEvents();
    }
}

int cursorColumnScore(const CHIron::Gui::TimelineWidget& widget, const int x)
{
    const QRect plot = widget.testPlotRect();
    QImage image(widget.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    const_cast<CHIron::Gui::TimelineWidget&>(widget).render(&painter);
    painter.end();

    int score = 0;
    for (int y = qMax(0, plot.top()); y <= qMin(image.height() - 1, plot.bottom()); ++y) {
        const QColor pixel = QColor::fromRgba(image.pixel(qBound(0, x, image.width() - 1), y));
        if (pixel.red() > 120 && pixel.green() > 70 && pixel.green() < 190 && pixel.blue() < 120) {
            ++score;
        }
    }

    return score;
}

void expectRenderedCursorAt(const CHIron::Gui::TimelineWidget& widget,
                            const int expectedX,
                            const QString& message)
{
    int bestScore = 0;
    int bestX = expectedX;
    for (int x = expectedX - 1; x <= expectedX + 1; ++x) {
        const int score = cursorColumnScore(widget, x);
        if (score > bestScore) {
            bestScore = score;
            bestX = x;
        }
    }

    expectNear(bestX, expectedX, 1, message);
    expect(bestScore >= 12,
           QStringLiteral("%1 Cursor color score was too low (score=%2).")
               .arg(message)
               .arg(bestScore));
}

QColor renderAddressPixelAt(const CHIron::Gui::AddressWidget& widget, const QPoint& point)
{
    QImage image(widget.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    const_cast<CHIron::Gui::AddressWidget&>(widget).render(&painter);
    painter.end();

    return QColor::fromRgba(image.pixel(qBound(0, point.x(), image.width() - 1),
                                        qBound(0, point.y(), image.height() - 1)));
}

void showLatencyForTest(CHIron::Gui::LatencyWidget& widget)
{
    widget.resize(900, 360);
    widget.ensurePolished();
    widget.buildView();
    widget.show();
    QApplication::processEvents();
    if (auto* tree = widget.findChild<QTreeWidget*>()) {
        tree->expandAll();
    }
    QApplication::processEvents();
}

void showLatencyDiffForTest(CHIron::Gui::LatencyDiffWidget& widget)
{
    widget.resize(780, 360);
    widget.ensurePolished();
    widget.show();
    QApplication::processEvents();
    if (auto* tree = widget.findChild<QTreeWidget*>()) {
        tree->expandAll();
        if (tree->header()) {
            tree->header()->resizeSection(4, 170);
        }
    }
    QApplication::processEvents();
}

QColor renderLatencyPixelAt(const CHIron::Gui::LatencyWidget& widget, const QPoint& point)
{
    QImage image(widget.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    const_cast<CHIron::Gui::LatencyWidget&>(widget).render(&painter);
    painter.end();

    return QColor::fromRgba(image.pixel(qBound(0, point.x(), image.width() - 1),
                                        qBound(0, point.y(), image.height() - 1)));
}

QImage renderLatencyImage(const CHIron::Gui::LatencyWidget& widget)
{
    QImage image(widget.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    const_cast<CHIron::Gui::LatencyWidget&>(widget).render(&painter);
    painter.end();
    return image;
}

bool latencyRegionHasVisibleNonWhitePixel(const CHIron::Gui::LatencyWidget& widget, const QRect& region)
{
    const QImage image = renderLatencyImage(widget);

    const QRect bounded = region.intersected(QRect(QPoint(0, 0), image.size()));
    for (int y = bounded.top(); y <= bounded.bottom(); y += 6) {
        for (int x = bounded.left(); x <= bounded.right(); x += 6) {
            const QColor pixel = QColor::fromRgba(image.pixel(x, y));
            if (pixel.alpha() > 0 && pixel != QColor(Qt::white)) {
                return true;
            }
        }
    }
    return false;
}

void showTransactionForTest(CHIron::Gui::TransactionWidget& widget)
{
    widget.resize(820, 260);
    widget.ensurePolished();
    widget.show();
    QApplication::processEvents();
}

QImage renderTransactionImage(const CHIron::Gui::TransactionWidget& widget)
{
    QImage image(widget.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    const_cast<CHIron::Gui::TransactionWidget&>(widget).render(&painter);
    painter.end();
    return image;
}

bool transactionImageHasVisibleContent(const CHIron::Gui::TransactionWidget& widget)
{
    const QImage image = renderTransactionImage(widget);
    for (int y = 0; y < image.height(); y += 8) {
        for (int x = 0; x < image.width(); x += 8) {
            const QColor pixel = QColor::fromRgba(image.pixel(x, y));
            if (pixel.alpha() > 0 && pixel != QColor(Qt::white)) {
                return true;
            }
        }
    }
    return false;
}

bool transactionImageHasNonWhiteNear(const CHIron::Gui::TransactionWidget& widget,
                                     const QPoint& center,
                                     const int radius)
{
    const QImage image = renderTransactionImage(widget);
    const QRect imageRect(QPoint(0, 0), image.size());
    const QRect region(center - QPoint(radius, radius),
                       QSize(radius * 2 + 1, radius * 2 + 1));
    const QRect bounded = region.intersected(imageRect);
    for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
        for (int x = bounded.left(); x <= bounded.right(); ++x) {
            const QColor pixel = QColor::fromRgba(image.pixel(x, y));
            if (pixel.alpha() > 0
                && pixel != QColor(Qt::white)
                && pixel != QColor(QStringLiteral("#FAFBFC"))
                && pixel != QColor(QStringLiteral("#FBFCFD"))) {
                return true;
            }
        }
    }
    return false;
}

bool transactionImageHasColorNear(const CHIron::Gui::TransactionWidget& widget,
                                  const QPoint& center,
                                  const int radius,
                                  const std::function<bool(const QColor&)>& predicate)
{
    const QImage image = renderTransactionImage(widget);
    const QRect imageRect(QPoint(0, 0), image.size());
    const QRect region(center - QPoint(radius, radius),
                       QSize(radius * 2 + 1, radius * 2 + 1));
    const QRect bounded = region.intersected(imageRect);
    for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
        for (int x = bounded.left(); x <= bounded.right(); ++x) {
            const QColor pixel = QColor::fromRgba(image.pixel(x, y));
            if (pixel.alpha() > 0 && predicate(pixel)) {
                return true;
            }
        }
    }
    return false;
}

void waitForTransactionSpans(CHIron::Gui::TransactionWidget& widget,
                             const int expectedSpans,
                             const QString& context)
{
    widget.buildView();
    QElapsedTimer waitTimer;
    waitTimer.start();
    while (widget.testSpanCount() != expectedSpans
           && !widget.testStatusText().startsWith(QStringLiteral("Transaction spans ready:"))
           && !widget.testStatusText().startsWith(QStringLiteral("No transaction spans"))
           && !widget.testStatusText().startsWith(QStringLiteral("No indexed transactions"))
           && waitTimer.elapsed() < 5000) {
        QApplication::processEvents();
        QThread::msleep(10);
    }
    QApplication::processEvents();
    expectEqual(widget.testSpanCount(), expectedSpans, context);
}

template<typename GlobalT>
void populateRetryTestTopology(GlobalT& global)
{
    global.TOPOLOGY.SetNode(1, CHI::Eb::Xact::NodeType::RN_F, "RN_F");
    global.TOPOLOGY.SetNode(2, CHI::Eb::Xact::NodeType::HN_F, "HN_F");
    global.TOPOLOGY.SetNode(3, CHI::Eb::Xact::NodeType::SN_F, "SN_F");
}

template<typename ReqFlit>
ReqFlit makeRetryReadRequest()
{
    ReqFlit request{};
    request.TgtID() = 2;
    request.SrcID() = 1;
    request.TxnID() = 7;
    request.Opcode() = CHI::Eb::Opcodes::REQ::ReadNoSnp;
    request.Size() = CHI::Eb::Sizes::B64;
    request.Addr() = 0x1000;
    request.AllowRetry() = 1;
    request.PCrdType() = 3;
    return request;
}

template<typename RspFlit>
RspFlit makeRetryAckResponse()
{
    RspFlit response{};
    response.TgtID() = 1;
    response.SrcID() = 2;
    response.TxnID() = 7;
    response.Opcode() = CHI::Eb::Opcodes::RSP::RetryAck;
    response.PCrdType() = 3;
    return response;
}

template<typename RspFlit>
RspFlit makePCrdGrantResponse()
{
    RspFlit response{};
    response.TgtID() = 1;
    response.SrcID() = 2;
    response.Opcode() = CHI::Eb::Opcodes::RSP::PCrdGrant;
    response.PCrdType() = 3;
    return response;
}

template<typename ReqFlit>
ReqFlit makeRetryReissuedRequest()
{
    ReqFlit request = makeRetryReadRequest<ReqFlit>();
    request.AllowRetry() = 0;
    request.PCrdType() = 3;
    return request;
}

struct TransactionTraceFixture {
    QTemporaryDir tempDir;
    QString tracePath;
    int transactionCount = 0;
};

TransactionTraceFixture makeIndexedReadNoSnpTrace(const QString& fileName,
                                                  const int transactionCount,
                                                  const bool manyBlocks = false)
{
    using XactTestConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 256, true, true, true>;
    using ReqFlit = CHI::Eb::Flits::REQ<XactTestConfig>;
    using DatFlit = CHI::Eb::Flits::DAT<XactTestConfig>;

    CLog::Parameters params;
    params.SetIssue(CLog::Issue::Eb);
    expect(params.SetNodeIdWidth(11), QStringLiteral("Failed to set test NodeIDWidth."));
    expect(params.SetReqAddrWidth(52), QStringLiteral("Failed to set test ReqAddrWidth."));
    expect(params.SetReqRSVDCWidth(32), QStringLiteral("Failed to set test ReqRSVDCWidth."));
    expect(params.SetDatRSVDCWidth(32), QStringLiteral("Failed to set test DatRSVDCWidth."));
    expect(params.SetDataWidth(256), QStringLiteral("Failed to set test DataWidth."));
    params.SetDataCheckPresent(true);
    params.SetPoisonPresent(true);
    params.SetMPAMPresent(true);

    const auto serializeReq = [](const ReqFlit& flit, TestFlitBitWriter& writer) {
        appendEbReqFlitForTest(flit, writer);
    };
    const auto serializeDat = [](const DatFlit& flit, TestFlitBitWriter& writer) {
        appendEbDatFlitForTest(flit, writer);
    };

    std::vector<CLog::CLogB::TagCHIRecords::Record> flatRecords;
    std::vector<std::vector<CLog::CLogB::TagCHIRecords::Record>> blocks;
    const int clampedCount = std::max(0, transactionCount);
    if (manyBlocks) {
        blocks.reserve(static_cast<std::size_t>((clampedCount + 31) / 32));
    } else {
        flatRecords.reserve(static_cast<std::size_t>(clampedCount * 3));
    }

    for (int index = 0; index < clampedCount; ++index) {
        ReqFlit request{};
        request.TgtID() = 16;
        request.SrcID() = static_cast<std::uint16_t>(1 + (index % 4));
        request.TxnID() = static_cast<std::uint16_t>(index & 0xFF);
        request.Opcode() = CHI::Eb::Opcodes::REQ::ReadNoSnp;
        request.Size() = CHI::Eb::Sizes::B64;
        request.Addr() = 0x1000 + static_cast<std::uint64_t>(index) * 0x100;
        request.AllowRetry() = 1;

        DatFlit data0{};
        data0.TgtID() = request.SrcID();
        data0.SrcID() = 16;
        data0.TxnID() = request.TxnID();
        data0.HomeNID() = 16;
        data0.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
        data0.RespErr() = CHI::Eb::RespErrs::OK;
        data0.Resp() = CHI::Eb::Resps::CompData_UC;
        data0.DBID() = static_cast<std::uint16_t>(index & 0xFF);
        data0.DataID() = 0;
        data0.BE() = 0xFFFFFFFFU;

        DatFlit data2 = data0;
        data2.DataID() = 2;

        std::vector<CLog::CLogB::TagCHIRecords::Record> triplet;
        triplet.reserve(3);
        triplet.push_back(makeSerializedRecord(CLog::Channel::TXREQ,
                                                request.SrcID(),
                                                1,
                                                request,
                                                serializeReq));
        triplet.push_back(makeSerializedRecord(CLog::Channel::RXDAT,
                                                request.SrcID(),
                                                1,
                                                data0,
                                                serializeDat));
        triplet.push_back(makeSerializedRecord(CLog::Channel::RXDAT,
                                                request.SrcID(),
                                                1,
                                                data2,
                                                serializeDat));

        if (manyBlocks) {
            if (blocks.empty() || blocks.back().size() >= 96) {
                blocks.emplace_back();
                blocks.back().reserve(96);
            }
            auto& block = blocks.back();
            block.insert(block.end(),
                         std::make_move_iterator(triplet.begin()),
                         std::make_move_iterator(triplet.end()));
        } else {
            flatRecords.insert(flatRecords.end(),
                               std::make_move_iterator(triplet.begin()),
                               std::make_move_iterator(triplet.end()));
        }
    }

    TransactionTraceFixture fixture;
    expect(fixture.tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    fixture.tracePath = fixture.tempDir.filePath(fileName);
    fixture.transactionCount = clampedCount;
    if (manyBlocks) {
        writeLengthOnlyTraceBlocks(fixture.tracePath, params, std::move(blocks));
    } else {
        writeTraceWithTopology(fixture.tracePath,
                               params,
                               {
                                   {CLog::NodeType::RN_F, 1},
                                   {CLog::NodeType::RN_F, 2},
                                   {CLog::NodeType::RN_F, 3},
                                   {CLog::NodeType::RN_F, 4},
                                   {CLog::NodeType::HN_F, 16},
                               },
                               std::move(flatRecords));
    }

    CHIron::Gui::CLogBTraceLoadError error;
    std::shared_ptr<CHIron::Gui::TraceSession> session;
    expect(CHIron::Gui::TraceSession::open(fixture.tracePath, session, error),
           error.summary.isEmpty()
               ? QStringLiteral("Trace session should open for indexed transaction fixture.")
               : error.summary);
    expect(session != nullptr, QStringLiteral("Indexed transaction fixture session should be created."));
    expect(session->buildXactionIndex(error),
           error.summary.isEmpty()
               ? QStringLiteral("Xaction index should build for indexed transaction fixture.")
               : error.summary);
    return fixture;
}

TransactionTraceFixture makeIndexedReadNoSnpAddressPatternTrace(
    const QString& fileName,
    const std::vector<std::uint64_t>& addresses,
    const bool reverseTransactionTimestamps = false)
{
    using XactTestConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 256, true, true, true>;
    using ReqFlit = CHI::Eb::Flits::REQ<XactTestConfig>;
    using DatFlit = CHI::Eb::Flits::DAT<XactTestConfig>;

    CLog::Parameters params;
    params.SetIssue(CLog::Issue::Eb);
    expect(params.SetNodeIdWidth(11), QStringLiteral("Failed to set patterned test NodeIDWidth."));
    expect(params.SetReqAddrWidth(52), QStringLiteral("Failed to set patterned test ReqAddrWidth."));
    expect(params.SetReqRSVDCWidth(32), QStringLiteral("Failed to set patterned test ReqRSVDCWidth."));
    expect(params.SetDatRSVDCWidth(32), QStringLiteral("Failed to set patterned test DatRSVDCWidth."));
    expect(params.SetDataWidth(256), QStringLiteral("Failed to set patterned test DataWidth."));
    params.SetDataCheckPresent(true);
    params.SetPoisonPresent(true);
    params.SetMPAMPresent(true);

    const auto serializeReq = [](const ReqFlit& flit, TestFlitBitWriter& writer) {
        appendEbReqFlitForTest(flit, writer);
    };
    const auto serializeDat = [](const DatFlit& flit, TestFlitBitWriter& writer) {
        appendEbDatFlitForTest(flit, writer);
    };

    std::vector<CLog::CLogB::TagCHIRecords::Record> records;
    records.reserve(addresses.size() * 3);
    for (int index = 0; index < static_cast<int>(addresses.size()); ++index) {
        const std::uint32_t baseShift = reverseTransactionTimestamps
            ? static_cast<std::uint32_t>((addresses.size() - static_cast<std::size_t>(index)) * 10U)
            : 1U;
        ReqFlit request{};
        request.TgtID() = 16;
        request.SrcID() = static_cast<std::uint16_t>(1 + (index % 4));
        request.TxnID() = static_cast<std::uint16_t>(index & 0xFF);
        request.Opcode() = CHI::Eb::Opcodes::REQ::ReadNoSnp;
        request.Size() = CHI::Eb::Sizes::B64;
        request.Addr() = addresses[static_cast<std::size_t>(index)];
        request.AllowRetry() = 1;

        DatFlit data0{};
        data0.TgtID() = request.SrcID();
        data0.SrcID() = 16;
        data0.TxnID() = request.TxnID();
        data0.HomeNID() = 16;
        data0.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
        data0.RespErr() = CHI::Eb::RespErrs::OK;
        data0.Resp() = CHI::Eb::Resps::CompData_UC;
        data0.DBID() = static_cast<std::uint16_t>(index & 0xFF);
        data0.DataID() = 0;
        data0.BE() = 0xFFFFFFFFU;

        DatFlit data2 = data0;
        data2.DataID() = 2;

        records.push_back(makeSerializedRecord(CLog::Channel::TXREQ,
                                                request.SrcID(),
                                                baseShift,
                                                request,
                                                serializeReq));
        records.push_back(makeSerializedRecord(CLog::Channel::RXDAT,
                                                request.SrcID(),
                                                baseShift + 1U,
                                                data0,
                                                serializeDat));
        records.push_back(makeSerializedRecord(CLog::Channel::RXDAT,
                                                request.SrcID(),
                                                baseShift + 2U,
                                                data2,
                                                serializeDat));
    }

    TransactionTraceFixture fixture;
    expect(fixture.tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    fixture.tracePath = fixture.tempDir.filePath(fileName);
    fixture.transactionCount = static_cast<int>(addresses.size());
    writeTraceWithTopology(fixture.tracePath,
                           params,
                           {
                               {CLog::NodeType::RN_F, 1},
                               {CLog::NodeType::RN_F, 2},
                               {CLog::NodeType::RN_F, 3},
                               {CLog::NodeType::RN_F, 4},
                               {CLog::NodeType::HN_F, 16},
                           },
                           std::move(records));

    CHIron::Gui::CLogBTraceLoadError error;
    std::shared_ptr<CHIron::Gui::TraceSession> session;
    expect(CHIron::Gui::TraceSession::open(fixture.tracePath, session, error),
           error.summary.isEmpty()
               ? QStringLiteral("Patterned trace session should open.")
               : error.summary);
    expect(session != nullptr, QStringLiteral("Patterned trace session should be created."));
    expect(session->buildXactionIndex(error),
           error.summary.isEmpty()
               ? QStringLiteral("Patterned trace should build an xaction index.")
               : error.summary);
    return fixture;
}

TransactionTraceFixture makeClipboardMaterializationTrace(const QString& fileName, const int rowCount)
{
    using XactTestConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 256, true, true, true>;
    using ReqFlit = CHI::Eb::Flits::REQ<XactTestConfig>;

    CLog::Parameters params;
    params.SetIssue(CLog::Issue::Eb);
    expect(params.SetNodeIdWidth(11), QStringLiteral("Failed to set materialization test NodeIDWidth."));
    expect(params.SetReqAddrWidth(52), QStringLiteral("Failed to set materialization test ReqAddrWidth."));
    expect(params.SetReqRSVDCWidth(32), QStringLiteral("Failed to set materialization test ReqRSVDCWidth."));
    expect(params.SetDatRSVDCWidth(32), QStringLiteral("Failed to set materialization test DatRSVDCWidth."));
    expect(params.SetDataWidth(256), QStringLiteral("Failed to set materialization test DataWidth."));
    params.SetDataCheckPresent(true);
    params.SetPoisonPresent(true);
    params.SetMPAMPresent(true);

    const auto serializeReq = [](const ReqFlit& flit, TestFlitBitWriter& writer) {
        appendEbReqFlitForTest(flit, writer);
    };

    std::vector<CLog::CLogB::TagCHIRecords::Record> records;
    const int clampedCount = std::max(0, rowCount);
    records.reserve(static_cast<std::size_t>(clampedCount));
    for (int row = 0; row < clampedCount; ++row) {
        ReqFlit request{};
        request.QoS() = static_cast<std::uint8_t>((row % 15) + 1);
        request.TgtID() = 16;
        request.SrcID() = static_cast<std::uint16_t>(1 + (row % 4));
        request.TxnID() = static_cast<std::uint16_t>(row & 0xFF);
        request.Opcode() = CHI::Eb::Opcodes::REQ::ReadNoSnp;
        request.Size() = CHI::Eb::Sizes::B64;
        request.Addr() = 0x1000 + static_cast<std::uint64_t>(row) * 0x40;
        request.AllowRetry() = static_cast<std::uint8_t>(row % 2);
        request.ExpCompAck() = static_cast<std::uint8_t>((row + 1) % 2);
        request.TraceTag() = static_cast<std::uint8_t>(row % 2);
        records.push_back(makeSerializedRecord(CLog::Channel::TXREQ,
                                                request.SrcID(),
                                                1,
                                                request,
                                                serializeReq));
    }

    TransactionTraceFixture fixture;
    expect(fixture.tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    fixture.tracePath = fixture.tempDir.filePath(fileName);
    fixture.transactionCount = clampedCount;
    writeTraceWithTopology(fixture.tracePath,
                           params,
                           {
                               {CLog::NodeType::RN_F, 1},
                               {CLog::NodeType::RN_F, 2},
                               {CLog::NodeType::RN_F, 3},
                               {CLog::NodeType::RN_F, 4},
                               {CLog::NodeType::HN_F, 16},
                           },
                           std::move(records));
    return fixture;
}

TransactionTraceFixture makeCacheReadCleanTrace(const QString& fileName,
                                                const int transactionCount)
{
    using XactTestConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 256, true, true, true>;
    using ReqFlit = CHI::Eb::Flits::REQ<XactTestConfig>;
    using DatFlit = CHI::Eb::Flits::DAT<XactTestConfig>;

    CLog::Parameters params;
    params.SetIssue(CLog::Issue::Eb);
    expect(params.SetNodeIdWidth(11), QStringLiteral("Failed to set cache test NodeIDWidth."));
    expect(params.SetReqAddrWidth(52), QStringLiteral("Failed to set cache test ReqAddrWidth."));
    expect(params.SetReqRSVDCWidth(32), QStringLiteral("Failed to set cache test ReqRSVDCWidth."));
    expect(params.SetDatRSVDCWidth(32), QStringLiteral("Failed to set cache test DatRSVDCWidth."));
    expect(params.SetDataWidth(256), QStringLiteral("Failed to set cache test DataWidth."));
    params.SetDataCheckPresent(true);
    params.SetPoisonPresent(true);
    params.SetMPAMPresent(true);

    const auto serializeReq = [](const ReqFlit& flit, TestFlitBitWriter& writer) {
        appendEbReqFlitForTest(flit, writer);
    };
    const auto serializeDat = [](const DatFlit& flit, TestFlitBitWriter& writer) {
        appendEbDatFlitForTest(flit, writer);
    };

    std::vector<CLog::CLogB::TagCHIRecords::Record> records;
    const int clampedCount = std::max(0, transactionCount);
    records.reserve(static_cast<std::size_t>(clampedCount * 3));
    for (int index = 0; index < clampedCount; ++index) {
        ReqFlit request{};
        request.TgtID() = 16;
        request.SrcID() = static_cast<std::uint16_t>(1 + index);
        request.TxnID() = static_cast<std::uint16_t>(index & 0xFF);
        request.Opcode() = CHI::Eb::Opcodes::REQ::ReadClean;
        request.Size() = CHI::Eb::Sizes::B64;
        request.Addr() = 0x1000 + static_cast<std::uint64_t>(index) * 0x100;
        request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_Allocate;
        request.SnpAttr() = CHI::Eb::SnpAttrs::Snoopable;
        request.AllowRetry() = 1;
        request.ExpCompAck() = 1;

        DatFlit data0{};
        data0.TgtID() = request.SrcID();
        data0.SrcID() = 16;
        data0.TxnID() = request.TxnID();
        data0.HomeNID() = 16;
        data0.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
        data0.RespErr() = CHI::Eb::RespErrs::OK;
        data0.Resp() = CHI::Eb::Resps::CompData_UC;
        data0.DBID() = static_cast<std::uint16_t>(index & 0xFF);
        data0.DataID() = 0;
        data0.BE() = 0xFFFFFFFFU;

        DatFlit data2 = data0;
        data2.DataID() = 2;

        records.push_back(makeSerializedRecord(CLog::Channel::TXREQ,
                                                request.SrcID(),
                                                1,
                                                request,
                                                serializeReq));
        records.push_back(makeSerializedRecord(CLog::Channel::RXDAT,
                                                request.SrcID(),
                                                1,
                                                data0,
                                                serializeDat));
        records.push_back(makeSerializedRecord(CLog::Channel::RXDAT,
                                                request.SrcID(),
                                                1,
                                                data2,
                                                serializeDat));
    }

    TransactionTraceFixture fixture;
    expect(fixture.tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    fixture.tracePath = fixture.tempDir.filePath(fileName);
    fixture.transactionCount = clampedCount;
    writeTraceWithTopology(fixture.tracePath,
                           params,
                           {
                               {CLog::NodeType::RN_F, 1},
                               {CLog::NodeType::RN_F, 2},
                               {CLog::NodeType::RN_F, 3},
                               {CLog::NodeType::RN_F, 4},
                               {CLog::NodeType::HN_F, 16},
                           },
                           std::move(records));
    return fixture;
}

TransactionTraceFixture makeCacheWriteEvictOrEvictCopyBackTrace(const QString& fileName,
                                                                 const bool invalidCopyBackReservedFields = false)
{
    using XactTestConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 256, true, true, true>;
    using ReqFlit = CHI::Eb::Flits::REQ<XactTestConfig>;
    using RspFlit = CHI::Eb::Flits::RSP<XactTestConfig>;
    using DatFlit = CHI::Eb::Flits::DAT<XactTestConfig>;

    CLog::Parameters params;
    params.SetIssue(CLog::Issue::Eb);
    expect(params.SetNodeIdWidth(11), QStringLiteral("Failed to set cache write test NodeIDWidth."));
    expect(params.SetReqAddrWidth(52), QStringLiteral("Failed to set cache write test ReqAddrWidth."));
    expect(params.SetReqRSVDCWidth(32), QStringLiteral("Failed to set cache write test ReqRSVDCWidth."));
    expect(params.SetDatRSVDCWidth(32), QStringLiteral("Failed to set cache write test DatRSVDCWidth."));
    expect(params.SetDataWidth(256), QStringLiteral("Failed to set cache write test DataWidth."));
    params.SetDataCheckPresent(true);
    params.SetPoisonPresent(true);
    params.SetMPAMPresent(true);

    const auto serializeReq = [](const ReqFlit& flit, TestFlitBitWriter& writer) {
        appendEbReqFlitForTest(flit, writer);
    };
    const auto serializeRsp = [](const RspFlit& flit, TestFlitBitWriter& writer) {
        appendEbRspFlitForTest(flit, writer);
    };
    const auto serializeDat = [](const DatFlit& flit, TestFlitBitWriter& writer) {
        appendEbDatFlitForTest(flit, writer);
    };

    std::vector<CLog::CLogB::TagCHIRecords::Record> records;
    records.reserve(6);

    ReqFlit read{};
    read.TgtID() = 16;
    read.SrcID() = 1;
    read.TxnID() = 1;
    read.Opcode() = CHI::Eb::Opcodes::REQ::ReadClean;
    read.Size() = CHI::Eb::Sizes::B64;
    read.Addr() = 0x1000;
    read.MemAttr() = CHI::Eb::MemAttrs::WriteBack_Allocate;
    read.SnpAttr() = CHI::Eb::SnpAttrs::Snoopable;
    read.AllowRetry() = 1;
    read.ExpCompAck() = 1;

    DatFlit readData{};
    readData.TgtID() = read.SrcID();
    readData.SrcID() = read.TgtID();
    readData.TxnID() = read.TxnID();
    readData.HomeNID() = read.TgtID();
    readData.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
    readData.RespErr() = CHI::Eb::RespErrs::OK;
    readData.Resp() = CHI::Eb::Resps::CompData_UC;
    readData.DBID() = 11;
    readData.DataID() = 0;
    readData.BE() = 0xFFFFFFFFU;
    DatFlit readDataRepeat = readData;
    readDataRepeat.DataID() = 2;

    ReqFlit writeEvict{};
    writeEvict.TgtID() = 16;
    writeEvict.SrcID() = 1;
    writeEvict.TxnID() = 2;
    writeEvict.Opcode() = CHI::Eb::Opcodes::REQ::WriteEvictOrEvict;
    writeEvict.Size() = CHI::Eb::Sizes::B64;
    writeEvict.Addr() = read.Addr();
    writeEvict.MemAttr() = CHI::Eb::MemAttrs::WriteBack_Allocate;
    writeEvict.SnpAttr() = CHI::Eb::SnpAttrs::Snoopable;
    writeEvict.AllowRetry() = 1;
    writeEvict.ExpCompAck() = 1;

    RspFlit compDbid{};
    compDbid.TgtID() = writeEvict.SrcID();
    compDbid.SrcID() = writeEvict.TgtID();
    compDbid.TxnID() = writeEvict.TxnID();
    compDbid.Opcode() = CHI::Eb::Opcodes::RSP::CompDBIDResp;
    compDbid.RespErr() = CHI::Eb::RespErrs::OK;
    compDbid.DBID() = 0;

    DatFlit copyBack{};
    copyBack.TgtID() = writeEvict.TgtID();
    copyBack.SrcID() = writeEvict.SrcID();
    copyBack.TxnID() = compDbid.DBID();
    copyBack.HomeNID() = static_cast<std::uint64_t>(invalidCopyBackReservedFields ? 16 : 0);
    copyBack.Opcode() = CHI::Eb::Opcodes::DAT::CopyBackWrData;
    copyBack.RespErr() = CHI::Eb::RespErrs::OK;
    copyBack.Resp() = CHI::Eb::Resps::CopyBackWrData_SC;
    copyBack.DataID() = 0;
    copyBack.BE() = 0xFFFFFFFFU;
    DatFlit copyBackRepeat = copyBack;
    copyBackRepeat.DataID() = 2;

    records.push_back(makeSerializedRecord(CLog::Channel::TXREQ, 1, 1, read, serializeReq));
    records.push_back(makeSerializedRecord(CLog::Channel::RXDAT, 1, 1, readData, serializeDat));
    records.push_back(makeSerializedRecord(CLog::Channel::RXDAT, 1, 1, readDataRepeat, serializeDat));
    records.push_back(makeSerializedRecord(CLog::Channel::TXREQ, 1, 1, writeEvict, serializeReq));
    records.push_back(makeSerializedRecord(CLog::Channel::RXRSP, 1, 1, compDbid, serializeRsp));
    records.push_back(makeSerializedRecord(CLog::Channel::TXDAT, 1, 1, copyBack, serializeDat));
    records.push_back(makeSerializedRecord(CLog::Channel::TXDAT, 1, 1, copyBackRepeat, serializeDat));

    TransactionTraceFixture fixture;
    expect(fixture.tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    fixture.tracePath = fixture.tempDir.filePath(fileName);
    fixture.transactionCount = 2;
    writeTraceWithTopology(fixture.tracePath,
                           params,
                           {
                               {CLog::NodeType::RN_F, 1},
                               {CLog::NodeType::HN_F, 16},
                           },
                           std::move(records));
    return fixture;
}

TransactionTraceFixture makeCacheReplayInitialStateDeniedTrace(const QString& fileName)
{
    using XactTestConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 256, true, true, true>;
    using ReqFlit = CHI::Eb::Flits::REQ<XactTestConfig>;

    CLog::Parameters params;
    params.SetIssue(CLog::Issue::Eb);
    expect(params.SetNodeIdWidth(11), QStringLiteral("Failed to set cache denial NodeIDWidth."));
    expect(params.SetReqAddrWidth(52), QStringLiteral("Failed to set cache denial ReqAddrWidth."));
    expect(params.SetReqRSVDCWidth(32), QStringLiteral("Failed to set cache denial ReqRSVDCWidth."));
    expect(params.SetDatRSVDCWidth(32), QStringLiteral("Failed to set cache denial DatRSVDCWidth."));
    expect(params.SetDataWidth(256), QStringLiteral("Failed to set cache denial DataWidth."));
    params.SetDataCheckPresent(true);
    params.SetPoisonPresent(true);
    params.SetMPAMPresent(true);

    const auto serializeReq = [](const ReqFlit& flit, TestFlitBitWriter& writer) {
        appendEbReqFlitForTest(flit, writer);
    };

    auto makeWriteEvict = [](const std::uint16_t txnId, const std::uint64_t address) {
        ReqFlit request{};
        request.TgtID() = 16;
        request.SrcID() = 1;
        request.TxnID() = txnId;
        request.Opcode() = CHI::Eb::Opcodes::REQ::WriteEvictOrEvict;
        request.Size() = CHI::Eb::Sizes::B64;
        request.Addr() = address;
        request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_Allocate;
        request.SnpAttr() = CHI::Eb::SnpAttrs::Snoopable;
        request.AllowRetry() = 1;
        request.ExpCompAck() = 1;
        return request;
    };

    std::vector<CLog::CLogB::TagCHIRecords::Record> records;
    records.push_back(makeSerializedRecord(CLog::Channel::TXREQ,
                                            1,
                                            1,
                                            makeWriteEvict(1, 0x1000),
                                            serializeReq));
    records.push_back(makeSerializedRecord(CLog::Channel::TXREQ,
                                            1,
                                            1,
                                            makeWriteEvict(2, 0x2000),
                                            serializeReq));

    TransactionTraceFixture fixture;
    expect(fixture.tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    fixture.tracePath = fixture.tempDir.filePath(fileName);
    fixture.transactionCount = 2;
    writeTraceWithTopology(fixture.tracePath,
                           params,
                           {
                               {CLog::NodeType::RN_F, 1},
                               {CLog::NodeType::HN_F, 16},
                           },
                           std::move(records));
    return fixture;
}

enum class RetryBoundarySplit {
    AfterRetryAck,
    AfterPCrdGrant,
};

TransactionTraceFixture makeRetryChunkBoundaryTrace(const QString& fileName,
                                                   const RetryBoundarySplit split)
{
    using XactTestConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 256, true, true, true>;
    using ReqFlit = CHI::Eb::Flits::REQ<XactTestConfig>;
    using RspFlit = CHI::Eb::Flits::RSP<XactTestConfig>;
    using DatFlit = CHI::Eb::Flits::DAT<XactTestConfig>;

    CLog::Parameters params;
    params.SetIssue(CLog::Issue::Eb);
    expect(params.SetNodeIdWidth(11), QStringLiteral("Failed to set retry boundary NodeIDWidth."));
    expect(params.SetReqAddrWidth(52), QStringLiteral("Failed to set retry boundary ReqAddrWidth."));
    expect(params.SetReqRSVDCWidth(32), QStringLiteral("Failed to set retry boundary ReqRSVDCWidth."));
    expect(params.SetDatRSVDCWidth(32), QStringLiteral("Failed to set retry boundary DatRSVDCWidth."));
    expect(params.SetDataWidth(256), QStringLiteral("Failed to set retry boundary DataWidth."));
    params.SetDataCheckPresent(true);
    params.SetPoisonPresent(true);
    params.SetMPAMPresent(true);

    const auto serializeReq = [](const ReqFlit& flit, TestFlitBitWriter& writer) {
        appendEbReqFlitForTest(flit, writer);
    };
    const auto serializeRsp = [](const RspFlit& flit, TestFlitBitWriter& writer) {
        appendEbRspFlitForTest(flit, writer);
    };
    const auto serializeDat = [](const DatFlit& flit, TestFlitBitWriter& writer) {
        appendEbDatFlitForTest(flit, writer);
    };

    ReqFlit original = makeRetryReadRequest<ReqFlit>();
    RspFlit retryAck = makeRetryAckResponse<RspFlit>();
    RspFlit pcrdGrant = makePCrdGrantResponse<RspFlit>();
    ReqFlit reissued = makeRetryReissuedRequest<ReqFlit>();

    DatFlit data0{};
    data0.TgtID() = reissued.SrcID();
    data0.SrcID() = reissued.TgtID();
    data0.TxnID() = reissued.TxnID();
    data0.HomeNID() = reissued.TgtID();
    data0.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
    data0.RespErr() = CHI::Eb::RespErrs::OK;
    data0.Resp() = CHI::Eb::Resps::CompData_UC;
    data0.DBID() = 9;
    data0.DataID() = 0;
    data0.BE() = 0xFFFFFFFFU;
    DatFlit data2 = data0;
    data2.DataID() = 2;

    std::vector<CLog::CLogB::TagCHIRecords::Record> records;
    records.reserve(split == RetryBoundarySplit::AfterRetryAck ? 4 : 6);
    records.push_back(makeSerializedRecord(CLog::Channel::TXREQ, 1, 1, original, serializeReq));
    records.push_back(makeSerializedRecord(CLog::Channel::RXRSP, 1, 1, retryAck, serializeRsp));
    records.push_back(makeSerializedRecord(CLog::Channel::RXRSP, 1, 1, pcrdGrant, serializeRsp));
    records.push_back(makeSerializedRecord(CLog::Channel::TXREQ, 1, 1, reissued, serializeReq));
    if (split == RetryBoundarySplit::AfterPCrdGrant) {
        records.push_back(makeSerializedRecord(CLog::Channel::RXDAT, 1, 1, data0, serializeDat));
        records.push_back(makeSerializedRecord(CLog::Channel::RXDAT, 1, 1, data2, serializeDat));
    }

    TransactionTraceFixture fixture;
    expect(fixture.tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    fixture.tracePath = fixture.tempDir.filePath(fileName);
    fixture.transactionCount = 1;
    writeTraceWithTopology(fixture.tracePath,
                           params,
                           {
                               {CLog::NodeType::RN_F, 1},
                               {CLog::NodeType::HN_F, 2},
                           },
                           std::move(records));
    return fixture;
}

TransactionTraceFixture makeCacheStateRoundTripTrace(const QString& fileName)
{
    using XactTestConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 256, true, true, true>;
    using ReqFlit = CHI::Eb::Flits::REQ<XactTestConfig>;
    using SnpFlit = CHI::Eb::Flits::SNP<XactTestConfig>;
    using RspFlit = CHI::Eb::Flits::RSP<XactTestConfig>;
    using DatFlit = CHI::Eb::Flits::DAT<XactTestConfig>;

    CLog::Parameters params;
    params.SetIssue(CLog::Issue::Eb);
    expect(params.SetNodeIdWidth(11), QStringLiteral("Failed to set cache round-trip NodeIDWidth."));
    expect(params.SetReqAddrWidth(52), QStringLiteral("Failed to set cache round-trip ReqAddrWidth."));
    expect(params.SetReqRSVDCWidth(32), QStringLiteral("Failed to set cache round-trip ReqRSVDCWidth."));
    expect(params.SetDatRSVDCWidth(32), QStringLiteral("Failed to set cache round-trip DatRSVDCWidth."));
    expect(params.SetDataWidth(256), QStringLiteral("Failed to set cache round-trip DataWidth."));
    params.SetDataCheckPresent(true);
    params.SetPoisonPresent(true);
    params.SetMPAMPresent(true);

    const auto serializeReq = [](const ReqFlit& flit, TestFlitBitWriter& writer) {
        appendEbReqFlitForTest(flit, writer);
    };
    const auto serializeSnp = [](const SnpFlit& flit, TestFlitBitWriter& writer) {
        appendEbSnpFlitForTest(flit, writer);
    };
    const auto serializeRsp = [](const RspFlit& flit, TestFlitBitWriter& writer) {
        appendEbRspFlitForTest(flit, writer);
    };
    const auto serializeDat = [](const DatFlit& flit, TestFlitBitWriter& writer) {
        appendEbDatFlitForTest(flit, writer);
    };

    std::vector<CLog::CLogB::TagCHIRecords::Record> records;
    records.reserve(8);

    auto appendReadClean = [&](const std::uint16_t txnId,
                               const std::uint8_t resp,
                               const std::uint16_t dbid) {
        ReqFlit request{};
        request.TgtID() = 16;
        request.SrcID() = 1;
        request.TxnID() = txnId;
        request.Opcode() = CHI::Eb::Opcodes::REQ::ReadClean;
        request.Size() = CHI::Eb::Sizes::B64;
        request.Addr() = 0x1000;
        request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_Allocate;
        request.SnpAttr() = CHI::Eb::SnpAttrs::Snoopable;
        request.AllowRetry() = 1;
        request.ExpCompAck() = 1;

        DatFlit data0{};
        data0.TgtID() = request.SrcID();
        data0.SrcID() = 16;
        data0.TxnID() = request.TxnID();
        data0.HomeNID() = 16;
        data0.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
        data0.RespErr() = CHI::Eb::RespErrs::OK;
        data0.Resp() = resp;
        data0.DBID() = dbid;
        data0.DataID() = 0;
        data0.BE() = 0xFFFFFFFFU;

        DatFlit data2 = data0;
        data2.DataID() = 2;

        records.push_back(makeSerializedRecord(CLog::Channel::TXREQ,
                                                request.SrcID(),
                                                1,
                                                request,
                                                serializeReq));
        records.push_back(makeSerializedRecord(CLog::Channel::RXDAT,
                                                request.SrcID(),
                                                1,
                                                data0,
                                                serializeDat));
        records.push_back(makeSerializedRecord(CLog::Channel::RXDAT,
                                                request.SrcID(),
                                                1,
                                                data2,
                                                serializeDat));
    };

    appendReadClean(1, CHI::Eb::Resps::CompData_UC, 11);

    SnpFlit invalidate{};
    invalidate.SrcID() = 16;
    invalidate.TxnID() = 2;
    invalidate.Opcode() = CHI::Eb::Opcodes::SNP::SnpMakeInvalid;
    invalidate.DoNotGoToSD() = 1;
    invalidate.Addr() = 0x1000 >> 3U;
    records.push_back(makeSerializedRecord(CLog::Channel::RXSNP,
                                            1,
                                            1,
                                            invalidate,
                                            serializeSnp));

    RspFlit invalidateResponse{};
    invalidateResponse.TgtID() = 16;
    invalidateResponse.SrcID() = 1;
    invalidateResponse.TxnID() = invalidate.TxnID();
    invalidateResponse.Opcode() = CHI::Eb::Opcodes::RSP::SnpResp;
    invalidateResponse.RespErr() = CHI::Eb::RespErrs::OK;
    invalidateResponse.Resp() = CHI::Eb::Resps::Snoop::I;
    records.push_back(makeSerializedRecord(CLog::Channel::TXRSP,
                                            1,
                                            1,
                                            invalidateResponse,
                                            serializeRsp));

    appendReadClean(3, CHI::Eb::Resps::CompData_UC, 13);

    TransactionTraceFixture fixture;
    expect(fixture.tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    fixture.tracePath = fixture.tempDir.filePath(fileName);
    fixture.transactionCount = 3;
    writeTraceWithTopology(fixture.tracePath,
                           params,
                           {
                               {CLog::NodeType::RN_F, 1},
                               {CLog::NodeType::HN_F, 16},
                           },
                           std::move(records));
    return fixture;
}

void testCacheLineLifetimeReplayBuildsReadCleanOpenEndedLifetimes()
{
    TransactionTraceFixture fixture =
        makeCacheReadCleanTrace(QStringLiteral("cache_lifetime_readclean.clogb"), 4);

    CHIron::Gui::CLogBTraceLoadError error;
    CHIron::Gui::CLogBTraceMetadata metadata;
    expect(CHIron::Gui::CLogBTraceLoader::scanFile(fixture.tracePath, metadata, error),
           error.summary.isEmpty()
               ? QStringLiteral("Cache replay fixture should scan as CLog.B.")
               : error.summary);

    std::vector<CHIron::Gui::CLogBTraceXactionIndexRecord> xactionRecords;
    expect(CHIron::Gui::CLogBTraceLoader::buildXactionIndex(
               fixture.tracePath,
               metadata,
               xactionRecords,
               error),
           error.summary.isEmpty()
               ? QStringLiteral("Cache replay fixture should build an E.b xaction index.")
               : error.summary);
    int indexedRecords = 0;
    QStringList xactionDenials;
    for (const auto& record : xactionRecords) {
        if (record.indexed) {
            ++indexedRecords;
        }
        if (record.processed && record.processedByJoint && record.denialName != QStringLiteral("ACCEPTED")) {
            xactionDenials.append(record.denialName);
        }
    }
    expect(indexedRecords > 0,
           QStringLiteral("Cache replay fixture should be accepted by the E.b xaction joint. Denials: %1")
               .arg(xactionDenials.join(QStringLiteral(", "))));

    std::vector<CHIron::Gui::CLogBTraceCacheLineLifetime> lifetimes;
    expect(CHIron::Gui::CLogBTraceLoader::buildCacheLineLifetimes(
               fixture.tracePath,
               metadata,
               lifetimes,
               error),
           error.summary.isEmpty()
               ? QStringLiteral("Cache replay should build RN cache-line lifetimes.")
               : error.summary);

    expectEqual(static_cast<int>(lifetimes.size()),
                fixture.transactionCount,
                QStringLiteral("Each ReadClean response entering UC state should create one cache lifetime."));

    for (int index = 0; index < fixture.transactionCount; ++index) {
        const auto& lifetime = lifetimes[static_cast<std::size_t>(index)];
        expectEqual(static_cast<int>(lifetime.rnNodeId),
                    1 + index,
                    QStringLiteral("Cache lifetimes should be grouped by the RN node that owns the line."));
        expectEqual(static_cast<int>(lifetime.lineAddress),
                    0x1000 + index * 0x100,
                    QStringLiteral("Cache replay should normalize addresses to 64-byte cache lines."));
        expect(lifetime.startStateText.contains(QStringLiteral("UC")),
               QStringLiteral("ReadClean CompData_UC should make the RN cache line alive."));
        expect(lifetime.openEnded,
               QStringLiteral("Still-live cache lines should close at EOF as open-ended lifetimes."));
        expectEqual(static_cast<std::uint64_t>(lifetime.endLogicalRow),
                    metadata.totalRecords - 1,
                    QStringLiteral("Open-ended cache lifetimes should close at the final logical row."));
    }
}

void testCacheLineStateReplayBuildsRequestedRnAddressSpans()
{
    TransactionTraceFixture fixture =
        makeCacheReadCleanTrace(QStringLiteral("cache_state_spans_readclean.clogb"), 4);

    CHIron::Gui::CLogBTraceLoadError error;
    CHIron::Gui::CLogBTraceMetadata metadata;
    expect(CHIron::Gui::CLogBTraceLoader::scanFile(fixture.tracePath, metadata, error),
           error.summary.isEmpty()
               ? QStringLiteral("Cache state replay fixture should scan as CLog.B.")
               : error.summary);

    std::vector<CHIron::Gui::CLogBTraceCacheLineStateSpan> spans;
    const std::uint64_t lineAddress =
        CHIron::Gui::CLogBTraceLoader::normalizeCacheLineAddress(0x1000 + 7);
    expect(CHIron::Gui::CLogBTraceLoader::buildCacheLineStateSpans(
               fixture.tracePath,
               metadata,
               CHIron::Gui::CLogBTraceCacheLineStateQuery{
                   .rnNodeId = 1,
                   .lineAddress = lineAddress,
               },
               spans,
               error),
           error.summary.isEmpty()
               ? QStringLiteral("Cache state replay should build spans for the requested RN/address.")
               : error.summary);

    expectEqual(static_cast<int>(spans.size()),
                1,
                QStringLiteral("ReadClean CompData_UC should create one state span for the requested RN/address."));
    const auto& span = spans.front();
    expectEqual(static_cast<int>(span.rnNodeId),
                1,
                QStringLiteral("State spans should retain the requested RN id."));
    expectEqual(static_cast<int>(span.lineAddress),
                0x1000,
                QStringLiteral("State replay should normalize byte addresses to 64-byte cache lines."));
    expect(span.stateText.contains(QStringLiteral("UC")),
           QStringLiteral("State replay should report the observed RN cache state."));
    expect(span.openEnded,
           QStringLiteral("Still-live cache state spans should close at EOF as open-ended."));
    expectEqual(static_cast<std::uint64_t>(span.endLogicalRow),
                metadata.totalRecords - 1,
                QStringLiteral("Open-ended cache state spans should close at the final logical row."));

    std::vector<CHIron::Gui::CLogBTraceCacheLineStateSpan> missingSpans;
    expect(CHIron::Gui::CLogBTraceLoader::buildCacheLineStateSpans(
               fixture.tracePath,
               metadata,
               CHIron::Gui::CLogBTraceCacheLineStateQuery{
                   .rnNodeId = 2,
                   .lineAddress = lineAddress,
               },
               missingSpans,
               error),
           error.summary.isEmpty()
               ? QStringLiteral("Cache state replay should accept queries with no matching state.")
               : error.summary);
    expect(missingSpans.empty(),
           QStringLiteral("State replay should filter out other RN lanes for the same address."));
}

void testCacheLineStateReplayAppliesReqTxdatCopyBack()
{
    using XactTestConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 256, true, true, true>;
    using ReqFlit = CHI::Eb::Flits::REQ<XactTestConfig>;
    using RspFlit = CHI::Eb::Flits::RSP<XactTestConfig>;
    using DatFlit = CHI::Eb::Flits::DAT<XactTestConfig>;

    CHI::Eb::Xact::Global<XactTestConfig> global;
    global.TOPOLOGY.SetNode(1, CHI::Eb::Xact::NodeType::RN_F, "RN_F");
    global.TOPOLOGY.SetNode(16, CHI::Eb::Xact::NodeType::HN_F, "HN_F");
    CHI::Eb::Xact::RNFJoint<XactTestConfig> rnJoint;
    CHI::Eb::Xact::RNCacheStateMap<XactTestConfig> stateMap;

    ReqFlit read{};
    read.TgtID() = 16;
    read.SrcID() = 1;
    read.TxnID() = 1;
    read.Opcode() = CHI::Eb::Opcodes::REQ::ReadClean;
    read.Size() = CHI::Eb::Sizes::B64;
    read.Addr() = 0x1000;
    read.MemAttr() = CHI::Eb::MemAttrs::WriteBack_Allocate;
    read.SnpAttr() = CHI::Eb::SnpAttrs::Snoopable;
    read.AllowRetry() = 1;
    read.ExpCompAck() = 1;

    expect(stateMap.NextTXREQ(0x1000, 1, read) == CHI::Eb::Xact::XactDenial::ACCEPTED,
           QStringLiteral("Core state map should accept ReadClean TXREQ."));
    std::shared_ptr<CHI::Eb::Xact::Xaction<XactTestConfig>> readXaction;
    expect(rnJoint.NextTXREQ(global, 1, read, &readXaction) == CHI::Eb::Xact::XactDenial::ACCEPTED,
           QStringLiteral("Core RN joint should accept ReadClean TXREQ."));

    DatFlit readData{};
    readData.TgtID() = read.SrcID();
    readData.SrcID() = read.TgtID();
    readData.TxnID() = read.TxnID();
    readData.HomeNID() = read.TgtID();
    readData.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
    readData.RespErr() = CHI::Eb::RespErrs::OK;
    readData.Resp() = CHI::Eb::Resps::CompData_UC;
    readData.DBID() = 11;
    readData.DataID() = 0;
    readData.BE() = 0xFFFFFFFFU;
    expect(rnJoint.NextRXDAT(global, 2, readData, &readXaction) == CHI::Eb::Xact::XactDenial::ACCEPTED,
           QStringLiteral("Core RN joint should accept ReadClean CompData."));
    expect(stateMap.NextRXDAT(0x1000, 2, *readXaction, readData) == CHI::Eb::Xact::XactDenial::ACCEPTED,
           QStringLiteral("Core state map should accept ReadClean CompData."));

    ReqFlit writeEvict{};
    writeEvict.TgtID() = 16;
    writeEvict.SrcID() = 1;
    writeEvict.TxnID() = 2;
    writeEvict.Opcode() = CHI::Eb::Opcodes::REQ::WriteEvictOrEvict;
    writeEvict.Size() = CHI::Eb::Sizes::B64;
    writeEvict.Addr() = read.Addr();
    writeEvict.MemAttr() = CHI::Eb::MemAttrs::WriteBack_Allocate;
    writeEvict.SnpAttr() = CHI::Eb::SnpAttrs::Snoopable;
    writeEvict.AllowRetry() = 1;
    writeEvict.ExpCompAck() = 1;
    expect(stateMap.NextTXREQ(0x1000, 4, writeEvict) == CHI::Eb::Xact::XactDenial::ACCEPTED,
           QStringLiteral("Core state map should accept WriteEvictOrEvict TXREQ."));
    std::shared_ptr<CHI::Eb::Xact::Xaction<XactTestConfig>> writeXaction;
    expect(rnJoint.NextTXREQ(global, 4, writeEvict, &writeXaction) == CHI::Eb::Xact::XactDenial::ACCEPTED,
           QStringLiteral("Core RN joint should accept WriteEvictOrEvict TXREQ."));

    RspFlit compDbid{};
    compDbid.TgtID() = writeEvict.SrcID();
    compDbid.SrcID() = writeEvict.TgtID();
    compDbid.TxnID() = writeEvict.TxnID();
    compDbid.Opcode() = CHI::Eb::Opcodes::RSP::CompDBIDResp;
    compDbid.RespErr() = CHI::Eb::RespErrs::OK;
    compDbid.DBID() = 0;
    expect(rnJoint.NextRXRSP(global, 5, compDbid, &writeXaction) == CHI::Eb::Xact::XactDenial::ACCEPTED,
           QStringLiteral("Core RN joint should accept CompDBIDResp."));
    expect(stateMap.NextRXRSP(0x1000, 5, *writeXaction, compDbid) == CHI::Eb::Xact::XactDenial::ACCEPTED,
           QStringLiteral("Core state map should accept CompDBIDResp."));

    DatFlit copyBack{};
    copyBack.TgtID() = writeEvict.TgtID();
    copyBack.SrcID() = writeEvict.SrcID();
    copyBack.TxnID() = compDbid.DBID();
    copyBack.HomeNID() = 0;
    copyBack.Opcode() = CHI::Eb::Opcodes::DAT::CopyBackWrData;
    copyBack.RespErr() = CHI::Eb::RespErrs::OK;
    copyBack.Resp() = CHI::Eb::Resps::CopyBackWrData_SC;
    copyBack.DataID() = 0;
    copyBack.BE() = 0xFFFFFFFFU;
    const auto coreTxdatDenial = rnJoint.NextTXDAT(global, 6, copyBack, &writeXaction);
    expect(coreTxdatDenial == CHI::Eb::Xact::XactDenial::ACCEPTED,
           QStringLiteral("Core RN joint should accept CopyBackWrData. denial=%1")
               .arg(coreTxdatDenial ? QString::fromLatin1(coreTxdatDenial->name)
                                    : QStringLiteral("null")));
    expect(stateMap.NextTXDAT(0x1000, 6, *writeXaction, copyBack) == CHI::Eb::Xact::XactDenial::ACCEPTED,
           QStringLiteral("Core state map should accept CopyBackWrData."));
    expectEqual(QString::fromStdString(stateMap.Get(0x1000).ToString()),
                QStringLiteral("I"),
                QStringLiteral("Core RNCacheStateMap should invalidate on WriteEvictOrEvict CopyBackWrData."));

    TransactionTraceFixture fixture =
        makeCacheWriteEvictOrEvictCopyBackTrace(QStringLiteral("cache_state_writeevict_txdat.clogb"));

    CHIron::Gui::CLogBTraceLoadError error;
    CHIron::Gui::CLogBTraceMetadata metadata;
    expect(CHIron::Gui::CLogBTraceLoader::scanFile(fixture.tracePath, metadata, error),
           error.summary.isEmpty()
               ? QStringLiteral("WriteEvict cache replay fixture should scan as CLog.B.")
               : error.summary);

    std::vector<CHIron::Gui::CLogBTraceCacheLineStateSpan> spans;
    expect(CHIron::Gui::CLogBTraceLoader::buildCacheLineStateSpans(
               fixture.tracePath,
               metadata,
               CHIron::Gui::CLogBTraceCacheLineStateQuery{
                   .rnNodeId = 1,
                   .lineAddress = 0x1000,
               },
               spans,
               error),
           error.summary.isEmpty()
               ? QStringLiteral("WriteEvict cache replay should build spans.")
               : error.summary);

    expect(!spans.empty(),
           QStringLiteral("WriteEvict CopyBackWrData should produce at least one live source span before I."));
    const auto& span = spans.back();
    QStringList spanDump;
    for (const auto& producedSpan : spans) {
        spanDump.push_back(QStringLiteral("%1-%2 %3 open=%4")
                               .arg(static_cast<qulonglong>(producedSpan.startLogicalRow))
                               .arg(static_cast<qulonglong>(producedSpan.endLogicalRow))
                               .arg(producedSpan.stateText)
                               .arg(producedSpan.openEnded ? QStringLiteral("true")
                                                           : QStringLiteral("false")));
    }
    expect(!span.openEnded,
           QStringLiteral("CopyBackWrData on requester TXDAT should invalidate the cache-line state. spans=%1")
               .arg(spanDump.join(QStringLiteral("; "))));
    expectEqual(static_cast<std::uint64_t>(span.endLogicalRow),
                static_cast<std::uint64_t>(5),
                QStringLiteral("State span should close on the first CopyBackWrData beat."));
    for (const auto& liveSpan : spans) {
        expect(!liveSpan.openEnded,
               QStringLiteral("WriteEvict CopyBackWrData should not leave any live UC/SC span open to EOF."));
    }

    std::vector<CHIron::Gui::CLogBTraceCacheLineLifetime> lifetimes;
    expect(CHIron::Gui::CLogBTraceLoader::buildCacheLineLifetimes(
               fixture.tracePath,
               metadata,
               lifetimes,
               error),
           error.summary.isEmpty()
               ? QStringLiteral("WriteEvict cache replay should build lifetimes.")
               : error.summary);
    expectEqual(static_cast<int>(lifetimes.size()),
                1,
                QStringLiteral("Lifetime replay should also close on requester TXDAT CopyBackWrData."));
    expect(!lifetimes.front().openEnded,
           QStringLiteral("Lifetime replay should not leave WriteEvict CopyBackWrData live to EOF."));
    expectEqual(static_cast<std::uint64_t>(lifetimes.front().endLogicalRow),
                static_cast<std::uint64_t>(5),
                QStringLiteral("Lifetime should close on the first CopyBackWrData beat."));
}

void testCacheLineStateReplayToleratesNonStateDatFieldDenials()
{
    TransactionTraceFixture fixture =
        makeCacheWriteEvictOrEvictCopyBackTrace(QStringLiteral("cache_state_writeevict_field_denial.clogb"),
                                                true);

    CHIron::Gui::CLogBTraceLoadError error;
    CHIron::Gui::CLogBTraceMetadata metadata;
    expect(CHIron::Gui::CLogBTraceLoader::scanFile(fixture.tracePath, metadata, error),
           error.summary.isEmpty()
               ? QStringLiteral("WriteEvict field-denial cache replay fixture should scan as CLog.B.")
               : error.summary);

    std::vector<CHIron::Gui::CLogBTraceCacheLineStateSpan> spans;
    expect(CHIron::Gui::CLogBTraceLoader::buildCacheLineStateSpans(
               fixture.tracePath,
               metadata,
               CHIron::Gui::CLogBTraceCacheLineStateQuery{
                   .rnNodeId = 1,
                   .lineAddress = 0x1000,
               },
               spans,
               error),
           error.summary.isEmpty()
               ? QStringLiteral("Cache replay should tolerate non-state DAT field denials.")
               : error.summary);

    expect(!spans.empty(),
           QStringLiteral("Field-denied CopyBackWrData fixture should produce a live span before invalidation."));
    expect(!spans.back().openEnded,
           QStringLiteral("Cache replay should still apply CopyBackWrData state transition when only DAT fields are denied."));
    expectEqual(static_cast<std::uint64_t>(spans.back().endLogicalRow),
                static_cast<std::uint64_t>(5),
                QStringLiteral("Field-denied CopyBackWrData should close the state span on the first beat."));
}

void testXactionIndexPersistsCacheStateReplayIssues()
{
    TransactionTraceFixture fixture =
        makeCacheReplayInitialStateDeniedTrace(QStringLiteral("cache_replay_denied_issue.clogb"));

    CHIron::Gui::CLogBTraceLoadError error;
    std::shared_ptr<CHIron::Gui::TraceSession> session;
    expect(CHIron::Gui::TraceSession::open(fixture.tracePath, session, error),
           error.summary.isEmpty()
               ? QStringLiteral("Cache replay denied TraceSession should open.")
               : error.summary);
    expect(session != nullptr,
           QStringLiteral("Cache replay denied TraceSession should be created."));
    expect(session->buildXactionIndex(error),
           error.summary.isEmpty()
               ? QStringLiteral("Cache replay denied Xaction index should build.")
               : error.summary);

    std::uint64_t persistedIssueCount = 0;
    expect(session->xactionIndexIssueCount(persistedIssueCount, error),
           error.summary.isEmpty()
               ? QStringLiteral("Cache replay denied persisted issue count should be readable.")
               : error.summary);
    std::vector<std::pair<std::uint64_t, std::uint64_t>> issueLoadProgress;
    std::vector<CHIron::Gui::TraceIssue> issues;
    expect(session->xactionIndexIssues(issues,
                                       error,
                                       {},
                                       [&issueLoadProgress](const std::uint64_t completedIssues,
                                                            const std::uint64_t totalIssues) {
                                           issueLoadProgress.emplace_back(completedIssues, totalIssues);
                                       }),
           error.summary.isEmpty()
               ? QStringLiteral("Persisted issue table should be readable.")
               : error.summary);
    expect(!issueLoadProgress.empty(),
           QStringLiteral("Persisted issue loading should report progress."));
    expectEqual(issueLoadProgress.front().first,
                static_cast<std::uint64_t>(0),
                QStringLiteral("Persisted issue loading should report an initial zero progress sample."));
    expectEqual(issueLoadProgress.front().second,
                persistedIssueCount,
                QStringLiteral("Persisted issue loading should report the issue table count as total progress."));
    std::uint64_t previousCompletedIssues = 0;
    for (const auto& progress : issueLoadProgress) {
        expectEqual(progress.second,
                    persistedIssueCount,
                    QStringLiteral("Persisted issue loading progress should keep a stable total."));
        expect(progress.first >= previousCompletedIssues,
               QStringLiteral("Persisted issue loading progress should be monotonic."));
        expect(progress.first <= progress.second,
               QStringLiteral("Persisted issue loading progress should not exceed total."));
        previousCompletedIssues = progress.first;
    }
    expectEqual(issueLoadProgress.back().first,
                persistedIssueCount,
                QStringLiteral("Persisted issue loading should report final completion."));
    const auto cacheIssue = std::find_if(issues.cbegin(), issues.cend(), [](const CHIron::Gui::TraceIssue& issue) {
        return issue.source == CHIron::Gui::TraceIssueSource::CacheStateReplay;
    });
    expect(cacheIssue != issues.cend(),
           QStringLiteral("Cache-state replay denial should be persisted into the Xaction index issue table."));
    expectEqual(cacheIssue->denialName,
                QStringLiteral("XACT_DENIED_STATE_INITIAL"),
                QStringLiteral("Cache issue denial name should come from RNCacheStateMap XactDenialEnum."));
    expect(cacheIssue->denialValue != 0,
           QStringLiteral("Cache issue should persist the RNCacheStateMap XactDenialEnum value."));
    expectEqual(static_cast<int>(cacheIssue->logicalRow),
                0,
                QStringLiteral("Cache replay initial-state denial should point at the denied TXREQ row."));
    expect(cacheIssue->details.contains(QStringLiteral("Cache-state replay denial")),
           QStringLiteral("Cache issue details should identify the cache replay source."));
}

void testXactionIndexAcceptsHomeToRequesterSnoop()
{
    TransactionTraceFixture fixture =
        makeCacheStateRoundTripTrace(QStringLiteral("xaction_home_to_rn_snoop.clogb"));

    CHIron::Gui::CLogBTraceLoadError error;
    std::shared_ptr<CHIron::Gui::TraceSession> session;
    expect(CHIron::Gui::TraceSession::open(fixture.tracePath, session, error),
           error.summary.isEmpty()
               ? QStringLiteral("HN-to-RN snoop TraceSession should open.")
               : error.summary);
    expect(session != nullptr,
           QStringLiteral("HN-to-RN snoop TraceSession should be created."));
    expect(session->buildXactionIndex(error),
           error.summary.isEmpty()
               ? QStringLiteral("HN-to-RN snoop Xaction index should build.")
               : error.summary);

    std::vector<CHIron::Gui::TraceIssue> issues;
    expect(session->xactionIndexIssues(issues, error),
           error.summary.isEmpty()
               ? QStringLiteral("HN-to-RN snoop issue table should be readable.")
               : error.summary);
    const auto routeDenial = std::find_if(issues.cbegin(), issues.cend(), [](const CHIron::Gui::TraceIssue& issue) {
        return issue.denialName == QStringLiteral("XACT_DENIED_SNP_NOT_FROM_HN_TO_RN");
    });
    expect(routeDenial == issues.cend(),
           QStringLiteral("Valid RXSNP from an HN-F source to an RN-F record node should not be route-denied."));

    const QString snpDebug = session->xactionDebugInfo(3);
    expect(snpDebug.contains(QStringLiteral("Joint path: RNFJoint::NextRXSNP")),
           QStringLiteral("Persisted snoop debug info should include its RNF joint path."));
    expect(snpDebug.contains(QStringLiteral("Denial: XACT_ACCEPTED")),
           QStringLiteral("Persisted snoop debug info should keep the accepted xaction result."));

    std::vector<CHIron::Gui::FlitRecord> rows;
    expect(session->readRows(3, 1, rows, error),
           error.summary.isEmpty()
               ? QStringLiteral("Accepted HN-to-RN snoop row should read back from the session.")
               : error.summary);
    expect(!rows.empty() && !CHIron::Gui::TransactionGroupKeys(rows.front()).empty(),
           QStringLiteral("Accepted HN-to-RN snoop row should keep persisted transaction grouping."));
}

void waitForTraceCacheMinimapLane(CHIron::Gui::MainWindow& window,
                                  const int laneIndex,
                                  const QString& context)
{
    const bool ready = waitForCondition([&window, laneIndex]() {
        const QVariantMap lane = window.testTraceCacheMinimapLaneAt(laneIndex);
        return !lane.isEmpty() && !lane.value(QStringLiteral("building")).toBool();
    }, 5000);
    expect(ready, context);
}

void waitForClipboardCacheMinimapLane(CHIron::Gui::MainWindow& window,
                                      const int laneIndex,
                                      const QString& context)
{
    const bool ready = waitForCondition([&window, laneIndex]() {
        return window.testClipboardCacheMinimapLaneCount() > laneIndex
            && window.testClipboardCacheMinimapSegmentCount(laneIndex) > 0;
    }, 5000);
    expect(ready, context);
}

void waitForClipboardAddressInsert(CHIron::Gui::MainWindow& window, const QString& context)
{
    const bool ready = waitForCondition([&window]() {
        return !window.testClipboardXactionAddressInsertActive();
    }, 10000);
    expect(ready, context);
}

void testTraceCacheMinimapMainTraceBuildsLaneOverlayAndSegments()
{
    TransactionTraceFixture fixture =
        makeCacheReadCleanTrace(QStringLiteral("trace_cache_minimap_main.clogb"), 4);

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(!window.testTraceCacheMinimapVisible(),
           QStringLiteral("Main trace Cache Map should be hidden by default."));
    expect(window.testLoadTraceFile(fixture.tracePath),
           QStringLiteral("Cache Map source trace should load."));
    QApplication::processEvents();

    expect(window.testAddTraceCacheMinimapLane(1, 0x103F),
           QStringLiteral("Adding a main trace Cache Map lane should accept decimal/hex-normalizable addresses."));
    waitForTraceCacheMinimapLane(window,
                                 0,
                                 QStringLiteral("Main trace Cache Map lane should finish building."));

    expect(window.testTraceCacheMinimapVisible(),
           QStringLiteral("Adding a lane should show the main trace Cache Map overlay."));
    expectEqual(window.testTraceCacheMinimapLaneCount(),
                1,
                QStringLiteral("Main trace Cache Map should contain the requested lane."));
    const QRect geometry = window.testTraceCacheMinimapGeometry();
    expect(geometry.isValid() && geometry.width() > 0 && geometry.height() > 0,
           QStringLiteral("Main trace Cache Map overlay should have visible geometry beside the table scrollbar."));
    const QVariantMap lane = window.testTraceCacheMinimapLaneAt(0);
    expectEqual(lane.value(QStringLiteral("rnNodeId")).toInt(),
                1,
                QStringLiteral("Cache Map lane should retain the requested RN id."));
    expectEqual(static_cast<int>(lane.value(QStringLiteral("lineAddress")).toULongLong()),
                0x1000,
                QStringLiteral("Cache Map lane should display the normalized cache-line address."));
    expect(window.testTraceCacheMinimapSegmentCount(0) > 0,
           QStringLiteral("Main trace Cache Map should paint at least one state segment for the requested line."));
    const QVariantMap segment = window.testTraceCacheMinimapSegmentAt(0, 0);
    expect(segment.value(QStringLiteral("stateText")).toString().contains(QStringLiteral("UC")),
           QStringLiteral("Cache Map segment should expose the cache-state label."));
    expect(segment.value(QStringLiteral("color")).value<QColor>() == QColor(QStringLiteral("#86EFAC")),
           QStringLiteral("Cache Map UC state should use its fixed lane color."));
    expect(segment.value(QStringLiteral("height")).toInt() > 0,
           QStringLiteral("Cache Map segment should have a drawable vertical span."));
}

void testTraceCacheMinimapTagsStackAndAnchorToLaneRightEdge()
{
    TransactionTraceFixture fixture =
        makeCacheReadCleanTrace(QStringLiteral("trace_cache_minimap_tags.clogb"), 4);

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(fixture.tracePath),
           QStringLiteral("Cache Map tag-layout source trace should load."));
    expect(window.testAddTraceCacheMinimapLane(1, 0x103F),
           QStringLiteral("Adding the first Cache Map lane should succeed."));
    waitForTraceCacheMinimapLane(window,
                                 0,
                                 QStringLiteral("First Cache Map lane should finish building."));
    expect(window.testAddTraceCacheMinimapLane(2, 0x113F),
           QStringLiteral("Adding the second Cache Map lane should succeed."));
    waitForTraceCacheMinimapLane(window,
                                 1,
                                 QStringLiteral("Second Cache Map lane should finish building."));

    const QRect firstTag = window.testTraceCacheMinimapTagRect(0);
    const QRect secondTag = window.testTraceCacheMinimapTagRect(1);
    const QRect firstLane = window.testTraceCacheMinimapLaneRect(0);
    const QRect secondLane = window.testTraceCacheMinimapLaneRect(1);
    const QRect minimapGeometry = window.testTraceCacheMinimapGeometry();
    expect(firstTag.isValid() && secondTag.isValid(),
           QStringLiteral("Cache Map lane tags should have valid rectangles."));
    expect(!firstTag.intersects(secondTag),
           QStringLiteral("Cache Map lane tags should stack without overlap."));
    expectEqual(firstTag.right(),
                firstLane.right(),
                QStringLiteral("The first Cache Map tag should stick to its lane right edge."));
    expectEqual(secondTag.right(),
                secondLane.right(),
                QStringLiteral("The second Cache Map tag should stick to its lane right edge."));
    expect(firstLane.intersects(firstTag),
           QStringLiteral("The first Cache Map lane should extend under its top tag."));
    expect(secondLane.intersects(secondTag),
           QStringLiteral("The second Cache Map lane should extend under its top tag."));
    expectEqual(firstLane.top(),
                0,
                QStringLiteral("Cache Map lane borders should start at the top of the overlay."));
    expectEqual(firstLane.height(),
                minimapGeometry.height(),
                QStringLiteral("Cache Map lane borders should span the full overlay height."));
}

void testTraceCacheMinimapAddingLaneDuringBuildFinishesBoth()
{
    TransactionTraceFixture fixture =
        makeCacheReadCleanTrace(QStringLiteral("trace_cache_minimap_add_during_build.clogb"), 64);

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(fixture.tracePath),
           QStringLiteral("Cache Map concurrent-add source trace should load."));
    expect(window.testAddTraceCacheMinimapLane(1, 0x103F),
           QStringLiteral("Adding the first Cache Map lane should start building."));
    expect(window.testAddTraceCacheMinimapLane(2, 0x113F),
           QStringLiteral("Adding a second Cache Map lane during the first build should succeed."));

    waitForTraceCacheMinimapLane(window,
                                 0,
                                 QStringLiteral("The interrupted first Cache Map lane should still finish building."));
    waitForTraceCacheMinimapLane(window,
                                 1,
                                 QStringLiteral("The second Cache Map lane should finish building."));

    const QVariantMap firstLane = window.testTraceCacheMinimapLaneAt(0);
    const QVariantMap secondLane = window.testTraceCacheMinimapLaneAt(1);
    expect(firstLane.value(QStringLiteral("hasBuildResult")).toBool(),
           QStringLiteral("The first Cache Map lane should retain a completed build result."));
    expect(secondLane.value(QStringLiteral("hasBuildResult")).toBool(),
           QStringLiteral("The second Cache Map lane should retain a completed build result."));
    expect(window.testTraceCacheMinimapSegmentCount(0) > 0,
           QStringLiteral("The first Cache Map lane should have visible state segments after interruption."));
    expect(window.testTraceCacheMinimapSegmentCount(1) > 0,
           QStringLiteral("The second Cache Map lane should have visible state segments."));
}

void testTraceCacheMinimapDeletingLaneKeepsRemainingBuildResult()
{
    TransactionTraceFixture fixture =
        makeCacheReadCleanTrace(QStringLiteral("trace_cache_minimap_delete_lane.clogb"), 4);

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(fixture.tracePath),
           QStringLiteral("Cache Map delete-lane source trace should load."));
    expect(window.testAddTraceCacheMinimapLane(1, 0x103F),
           QStringLiteral("Adding the first Cache Map lane should succeed."));
    waitForTraceCacheMinimapLane(window,
                                 0,
                                 QStringLiteral("First Cache Map lane should finish before deletion."));
    expect(window.testAddTraceCacheMinimapLane(2, 0x113F),
           QStringLiteral("Adding the second Cache Map lane should succeed."));
    waitForTraceCacheMinimapLane(window,
                                 1,
                                 QStringLiteral("Second Cache Map lane should finish before deletion."));

    window.testRemoveTraceCacheMinimapLane(0);
    QApplication::processEvents();

    expectEqual(window.testTraceCacheMinimapLaneCount(),
                1,
                QStringLiteral("Deleting one Cache Map lane should leave the other lane."));
    const QVariantMap remainingLane = window.testTraceCacheMinimapLaneAt(0);
    expect(!remainingLane.value(QStringLiteral("building")).toBool(),
           QStringLiteral("Deleting a completed Cache Map lane should not rebuild the remaining lane."));
    expect(remainingLane.value(QStringLiteral("hasBuildResult")).toBool(),
           QStringLiteral("Deleting a completed Cache Map lane should preserve the remaining lane build result."));
    expect(window.testTraceCacheMinimapSegmentCount(0) > 0,
           QStringLiteral("Deleting a completed Cache Map lane should preserve remaining visible segments."));
}

void testTraceCacheMinimapStateJumpActions()
{
    constexpr int kJumpStateUC = 0;
    constexpr int kJumpStateSC = 4;
    constexpr int kJumpDirectionFirst = 0;
    constexpr int kJumpDirectionPrevious = 1;
    constexpr int kJumpDirectionNext = 2;
    constexpr int kJumpDirectionLast = 3;

    TransactionTraceFixture fixture =
        makeCacheStateRoundTripTrace(QStringLiteral("trace_cache_minimap_state_jumps.clogb"));

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(fixture.tracePath),
           QStringLiteral("Cache Map state-jump fixture should load."));
    expect(window.testAddTraceCacheMinimapLane(1, 0x103F),
           QStringLiteral("Adding a Cache Map state-jump lane should succeed."));
    waitForTraceCacheMinimapLane(window,
                                 0,
                                 QStringLiteral("Cache Map state-jump lane should build."));
    expectEqual(window.testTraceCacheMinimapSegmentCount(0),
                2,
                QStringLiteral("State-jump fixture should expose two UC spans."));

    const QVariantMap firstSegment = window.testTraceCacheMinimapSegmentAt(0, 0);
    const QVariantMap secondSegment = window.testTraceCacheMinimapSegmentAt(0, 1);
    const int firstRow = static_cast<int>(firstSegment.value(QStringLiteral("startLogicalRow")).toULongLong());
    const int secondRow = static_cast<int>(secondSegment.value(QStringLiteral("startLogicalRow")).toULongLong());

    expect(window.testTraceCacheMinimapJumpAvailable(0, kJumpStateUC, kJumpDirectionFirst, 0),
           QStringLiteral("First UC should be enabled for a lane with UC segments."));
    expect(window.testTraceCacheMinimapJumpAvailable(0, kJumpStateUC, kJumpDirectionLast, 0),
           QStringLiteral("Last UC should be enabled for a lane with UC segments."));
    expect(!window.testTraceCacheMinimapJumpAvailable(0, kJumpStateSC, kJumpDirectionFirst, 0),
           QStringLiteral("First SC should be disabled when the lane has no SC segments."));
    expect(!window.testTraceCacheMinimapJumpAvailable(0, kJumpStateUC, kJumpDirectionPrevious, firstRow),
           QStringLiteral("Previous UC should be disabled before the first UC segment."));
    expect(window.testTraceCacheMinimapJumpAvailable(0, kJumpStateUC, kJumpDirectionNext, firstRow),
           QStringLiteral("Next UC should be enabled when a later UC segment exists."));

    expect(window.testTriggerTraceCacheMinimapJump(0, kJumpStateUC, kJumpDirectionFirst, secondRow),
           QStringLiteral("Triggering First UC should succeed."));
    expectEqual(window.testSelectedLogicalRow(),
                firstRow,
                QStringLiteral("First UC should jump to the first matching segment."));

    expect(window.testTriggerTraceCacheMinimapJump(0, kJumpStateUC, kJumpDirectionNext, firstRow),
           QStringLiteral("Triggering Next UC should succeed."));
    expectEqual(window.testSelectedLogicalRow(),
                secondRow,
                QStringLiteral("Next UC should jump to the next matching segment after the reference row."));

    expect(window.testTriggerTraceCacheMinimapJump(0, kJumpStateUC, kJumpDirectionPrevious, secondRow),
           QStringLiteral("Triggering Previous UC should succeed."));
    expectEqual(window.testSelectedLogicalRow(),
                firstRow,
                QStringLiteral("Previous UC should jump to the previous matching segment before the reference row."));

    expect(window.testTriggerTraceCacheMinimapJump(0, kJumpStateUC, kJumpDirectionLast, firstRow),
           QStringLiteral("Triggering Last UC should succeed."));
    expectEqual(window.testSelectedLogicalRow(),
                secondRow,
                QStringLiteral("Last UC should jump to the last matching segment."));
}

void testTraceCacheMinimapChangeJumpActions()
{
    constexpr int kChangeDirectionFirst = 0;
    constexpr int kChangeDirectionPrevious = 1;
    constexpr int kChangeDirectionNext = 2;
    constexpr int kChangeDirectionLast = 3;

    TransactionTraceFixture fixture =
        makeCacheStateRoundTripTrace(QStringLiteral("trace_cache_minimap_change_jumps.clogb"));

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(fixture.tracePath),
           QStringLiteral("Cache Map change-jump fixture should load."));
    expect(window.testAddTraceCacheMinimapLane(1, 0x103F),
           QStringLiteral("Adding a Cache Map change-jump lane should succeed."));
    waitForTraceCacheMinimapLane(window,
                                 0,
                                 QStringLiteral("Cache Map change-jump lane should build."));
    expectEqual(window.testTraceCacheMinimapSegmentCount(0),
                2,
                QStringLiteral("Change-jump fixture should expose two coalesced spans."));

    const QVariantMap firstSegment = window.testTraceCacheMinimapSegmentAt(0, 0);
    const QVariantMap secondSegment = window.testTraceCacheMinimapSegmentAt(0, 1);
    const int firstRow = static_cast<int>(firstSegment.value(QStringLiteral("startLogicalRow")).toULongLong());
    const int invalidRow = static_cast<int>(firstSegment.value(QStringLiteral("endLogicalRow")).toULongLong());
    const int secondRow = static_cast<int>(secondSegment.value(QStringLiteral("startLogicalRow")).toULongLong());
    expect(secondSegment.value(QStringLiteral("openEnded")).toBool(),
           QStringLiteral("The second round-trip segment should be open-ended."));

    expect(window.testTraceCacheMinimapChangeJumpAvailable(0, kChangeDirectionFirst, firstRow),
           QStringLiteral("First change should include the initial I-to-live transition."));
    expect(window.testTraceCacheMinimapChangeJumpAvailable(0, kChangeDirectionPrevious, secondRow),
           QStringLiteral("Previous change should include the live-to-I transition before the second span."));
    expect(window.testTraceCacheMinimapChangeJumpAvailable(0, kChangeDirectionNext, firstRow),
           QStringLiteral("Next change should include the live-to-I transition after the first span."));
    expect(window.testTraceCacheMinimapChangeJumpAvailable(0, kChangeDirectionLast, firstRow),
           QStringLiteral("Last change should be enabled when the lane has a change segment."));
    expect(!window.testTraceCacheMinimapChangeJumpAvailable(0, kChangeDirectionNext, secondRow),
           QStringLiteral("Next change should be disabled after the final open-ended I-to-live transition."));

    expect(window.testTriggerTraceCacheMinimapChangeJump(0, kChangeDirectionFirst, firstRow),
           QStringLiteral("Triggering First change should succeed."));
    expectEqual(window.testSelectedLogicalRow(),
                firstRow,
                QStringLiteral("First change should jump to the initial I-to-live row."));

    expect(window.testTriggerTraceCacheMinimapChangeJump(0, kChangeDirectionPrevious, secondRow),
           QStringLiteral("Triggering Previous change should succeed."));
    expectEqual(window.testSelectedLogicalRow(),
                invalidRow,
                QStringLiteral("Previous change should jump to the live-to-I row before the second span."));

    expect(window.testTriggerTraceCacheMinimapChangeJump(0, kChangeDirectionNext, firstRow),
           QStringLiteral("Triggering Next change should succeed."));
    expectEqual(window.testSelectedLogicalRow(),
                invalidRow,
                QStringLiteral("Next change should jump to the first live-to-I row."));

    expect(window.testTriggerTraceCacheMinimapChangeJump(0, kChangeDirectionLast, firstRow),
           QStringLiteral("Triggering Last change should succeed."));
    expectEqual(window.testSelectedLogicalRow(),
                secondRow,
                QStringLiteral("Last change should jump to the final I-to-live row for an open-ended span."));

    TransactionTraceFixture singleSpanFixture =
        makeCacheReadCleanTrace(QStringLiteral("trace_cache_minimap_change_single.clogb"), 1);
    CHIron::Gui::MainWindow singleSpanWindow;
    singleSpanWindow.resize(1200, 720);
    singleSpanWindow.show();
    QApplication::processEvents();

    expect(singleSpanWindow.testLoadTraceFile(singleSpanFixture.tracePath),
           QStringLiteral("Single-span Cache Map fixture should load."));
    expect(singleSpanWindow.testAddTraceCacheMinimapLane(1, 0x103F),
           QStringLiteral("Adding a single-span Cache Map lane should succeed."));
    waitForTraceCacheMinimapLane(singleSpanWindow,
                                 0,
                                 QStringLiteral("Single-span Cache Map lane should build."));
    expectEqual(singleSpanWindow.testTraceCacheMinimapSegmentCount(0),
                1,
                QStringLiteral("Single-span fixture should expose one coalesced span."));
    const QVariantMap singleSegment = singleSpanWindow.testTraceCacheMinimapSegmentAt(0, 0);
    const int singleStartRow =
        static_cast<int>(singleSegment.value(QStringLiteral("startLogicalRow")).toULongLong());
    expect(singleSpanWindow.testTraceCacheMinimapChangeJumpAvailable(0, kChangeDirectionFirst, 0),
           QStringLiteral("First change should be enabled for a one-segment lane with an I-to-live transition."));
    expect(!singleSpanWindow.testTraceCacheMinimapChangeJumpAvailable(0, kChangeDirectionPrevious, 0),
           QStringLiteral("Previous change should be disabled for a one-segment lane."));
    expect(singleSpanWindow.testTraceCacheMinimapChangeJumpAvailable(0, kChangeDirectionNext, 0),
           QStringLiteral("Next change should find the initial I-to-live transition before its row."));
    expect(!singleSpanWindow.testTraceCacheMinimapChangeJumpAvailable(0, kChangeDirectionNext, singleStartRow),
           QStringLiteral("Next change should be disabled after the only I-to-live transition."));
    expect(singleSpanWindow.testTraceCacheMinimapChangeJumpAvailable(0, kChangeDirectionLast, 0),
           QStringLiteral("Last change should be enabled for a one-segment lane with an I-to-live transition."));
}

void testTraceCacheMinimapClipboardBuildsFromSourceRows()
{
    TransactionTraceFixture fixture =
        makeCacheReadCleanTrace(QStringLiteral("trace_cache_minimap_clipboard.clogb"), 2);

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(!window.testClipboardCacheMinimapVisible(),
           QStringLiteral("Clipboard Cache Map should be hidden by default."));
    expect(window.testLoadTraceFile(fixture.tracePath),
           QStringLiteral("Clipboard Cache Map source trace should load."));
    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Session);
    window.testSelectLogicalRow(1);
    expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Session),
           QStringLiteral("Clipboard Cache Map should have a source-backed row to map."));
    QApplication::processEvents();

    expect(window.testAddClipboardCacheMinimapLane(1, 0x103F),
           QStringLiteral("Adding a Clipboard Cache Map lane should succeed."));
    waitForClipboardCacheMinimapLane(window,
                                     0,
                                     QStringLiteral("Clipboard Cache Map lane should build from the source trace session."));

    expect(window.testClipboardCacheMinimapVisible(),
           QStringLiteral("Adding a Clipboard lane should show the Clipboard Cache Map overlay."));
    expectEqual(window.testClipboardCacheMinimapLaneCount(),
                1,
                QStringLiteral("Clipboard Cache Map should contain the requested lane."));
}

void testTraceCacheMinimapClipboardStateJumpActions()
{
    constexpr int kJumpStateUC = 0;
    constexpr int kJumpStateSC = 4;
    constexpr int kJumpDirectionFirst = 0;
    constexpr int kJumpDirectionNext = 2;

    TransactionTraceFixture fixture =
        makeCacheStateRoundTripTrace(QStringLiteral("trace_cache_minimap_clipboard_state_jumps.clogb"));

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(fixture.tracePath),
           QStringLiteral("Clipboard Cache Map state-jump fixture should load."));
    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Session);
    const int rowsToInsert[] = {1, 6};
    for (const int logicalRow : rowsToInsert) {
        window.testSelectLogicalRow(logicalRow);
        expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Session),
               QStringLiteral("Clipboard state-jump source row %1 insertion should succeed.").arg(logicalRow));
    }
    QApplication::processEvents();

    expect(window.testAddClipboardCacheMinimapLane(1, 0x103F),
           QStringLiteral("Adding a Clipboard Cache Map state-jump lane should succeed."));
    waitForClipboardCacheMinimapLane(window,
                                     0,
                                     QStringLiteral("Clipboard Cache Map state-jump lane should build."));
    expectEqual(window.testClipboardCacheMinimapSegmentCount(0),
                2,
                QStringLiteral("Clipboard state-jump fixture should expose two projected UC spans."));

    expect(window.testClipboardCacheMinimapJumpAvailable(0, kJumpStateUC, kJumpDirectionNext, 0),
           QStringLiteral("Clipboard Next UC should be enabled after the first visible row."));
    expect(!window.testClipboardCacheMinimapJumpAvailable(0, kJumpStateSC, kJumpDirectionFirst, 0),
           QStringLiteral("Clipboard First SC should be disabled when the lane has no SC segments."));

    expect(window.testTriggerClipboardCacheMinimapJump(0, kJumpStateUC, kJumpDirectionNext, 0),
           QStringLiteral("Triggering Clipboard Next UC should succeed."));
    expectEqual(window.testSelectedLogicalRow(),
                6,
                QStringLiteral("Clipboard Next UC should activate the second projected Clipboard row."));

    expect(window.testTriggerClipboardCacheMinimapJump(0, kJumpStateUC, kJumpDirectionFirst, 1),
           QStringLiteral("Triggering Clipboard First UC should succeed."));
    expectEqual(window.testSelectedLogicalRow(),
                1,
                QStringLiteral("Clipboard First UC should activate the first projected Clipboard row."));
}

void testTraceCacheMinimapClipboardChangeJumpActions()
{
    constexpr int kChangeDirectionFirst = 0;
    constexpr int kChangeDirectionPrevious = 1;
    constexpr int kChangeDirectionNext = 2;
    constexpr int kChangeDirectionLast = 3;

    TransactionTraceFixture fixture =
        makeCacheStateRoundTripTrace(QStringLiteral("trace_cache_minimap_clipboard_change_jumps.clogb"));

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(fixture.tracePath),
           QStringLiteral("Clipboard Cache Map change-jump fixture should load."));
    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Session);
    const int rowsToInsert[] = {1, 6};
    for (const int logicalRow : rowsToInsert) {
        window.testSelectLogicalRow(logicalRow);
        expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Session),
               QStringLiteral("Clipboard change-jump source row %1 insertion should succeed.").arg(logicalRow));
    }
    QApplication::processEvents();

    expect(window.testAddClipboardCacheMinimapLane(1, 0x103F),
           QStringLiteral("Adding a Clipboard Cache Map change-jump lane should succeed."));
    waitForClipboardCacheMinimapLane(window,
                                     0,
                                     QStringLiteral("Clipboard Cache Map change-jump lane should build."));
    expectEqual(window.testClipboardCacheMinimapSegmentCount(0),
                2,
                QStringLiteral("Clipboard change-jump fixture should expose two projected same-state spans."));

    const QVariantMap firstSegment = window.testClipboardCacheMinimapSegmentAt(0, 0);
    const QVariantMap secondSegment = window.testClipboardCacheMinimapSegmentAt(0, 1);
    expectEqual(firstSegment.value(QStringLiteral("stateText")).toString(),
                secondSegment.value(QStringLiteral("stateText")).toString(),
                QStringLiteral("The Clipboard fixture should return to the same displayed state."));
    expect(secondSegment.value(QStringLiteral("splitBefore")).toBool(),
           QStringLiteral("The second Clipboard segment should be a split change even with matching text/color."));

    expect(window.testClipboardCacheMinimapChangeJumpAvailable(0, kChangeDirectionFirst, 0),
           QStringLiteral("Clipboard First change should include the initial I-to-live transition."));
    expect(window.testClipboardCacheMinimapChangeJumpAvailable(0, kChangeDirectionPrevious, 1),
           QStringLiteral("Clipboard Previous change should include the first projected I-to-live transition."));
    expect(window.testClipboardCacheMinimapChangeJumpAvailable(0, kChangeDirectionNext, 0),
           QStringLiteral("Clipboard Next change should include the next projected I-to-live transition."));
    expect(window.testClipboardCacheMinimapChangeJumpAvailable(0, kChangeDirectionLast, 0),
           QStringLiteral("Clipboard Last change should be enabled when a later projected segment exists."));

    expect(window.testTriggerClipboardCacheMinimapChangeJump(0, kChangeDirectionFirst, 0),
           QStringLiteral("Triggering Clipboard First change should succeed."));
    expectEqual(window.testSelectedLogicalRow(),
                1,
                QStringLiteral("Clipboard First change should activate the first projected I-to-live row."));

    expect(window.testTriggerClipboardCacheMinimapChangeJump(0, kChangeDirectionNext, 0),
           QStringLiteral("Triggering Clipboard Next change should succeed."));
    expectEqual(window.testSelectedLogicalRow(),
                6,
                QStringLiteral("Clipboard Next change should activate the second projected source row."));

    expect(window.testTriggerClipboardCacheMinimapChangeJump(0, kChangeDirectionLast, 0),
           QStringLiteral("Triggering Clipboard Last change should succeed."));
    expectEqual(window.testSelectedLogicalRow(),
                6,
                QStringLiteral("Clipboard Last change should activate the final projected source row."));
}

void testTraceCacheMinimapClipboardMergesRowsInsideSameSourceState()
{
    TransactionTraceFixture fixture =
        makeCacheReadCleanTrace(QStringLiteral("trace_cache_minimap_clipboard_merge.clogb"), 1);

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(fixture.tracePath),
           QStringLiteral("Clipboard Cache Map merge fixture should load."));
    expect(window.testAddTraceCacheMinimapLane(1, 0x103F),
           QStringLiteral("Main trace Cache Map lane should be available for comparison."));
    waitForTraceCacheMinimapLane(window,
                                 0,
                                 QStringLiteral("Main trace Cache Map lane should build before comparison."));
    const QVariantMap mainSegment = window.testTraceCacheMinimapSegmentAt(0, 0);

    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Session);
    window.testSelectLogicalRow(1);
    expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Session),
           QStringLiteral("First Clipboard data row insertion should succeed."));
    window.testSelectLogicalRow(2);
    expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Session),
           QStringLiteral("Second Clipboard data row insertion should succeed."));
    QApplication::processEvents();

    expect(window.testAddClipboardCacheMinimapLane(1, 0x103F),
           QStringLiteral("Adding a Clipboard Cache Map merge lane should succeed."));
    waitForClipboardCacheMinimapLane(window,
                                     0,
                                     QStringLiteral("Clipboard Cache Map merge lane should build."));

    expectEqual(window.testClipboardCacheMinimapSegmentCount(0),
                1,
                QStringLiteral("Clipboard rows in the same unchanged source state should draw one segment."));
    const QVariantMap clipboardSegment = window.testClipboardCacheMinimapSegmentAt(0, 0);
    expectEqual(clipboardSegment.value(QStringLiteral("startVisibleRow")).toInt(),
                0,
                QStringLiteral("Merged Clipboard segment should start at the first visible Clipboard row."));
    expectEqual(clipboardSegment.value(QStringLiteral("endVisibleRow")).toInt(),
                1,
                QStringLiteral("Merged Clipboard segment should end at the last visible Clipboard row."));
    expectEqual(clipboardSegment.value(QStringLiteral("stateText")).toString(),
                mainSegment.value(QStringLiteral("stateText")).toString(),
                QStringLiteral("Clipboard Cache Map state text should match the main trace projection."));
    expectEqual(clipboardSegment.value(QStringLiteral("stateMask")).toInt(),
                mainSegment.value(QStringLiteral("stateMask")).toInt(),
                QStringLiteral("Clipboard Cache Map state mask should match the main trace projection."));
    expectEqual(clipboardSegment.value(QStringLiteral("color")).value<QColor>().name(),
                mainSegment.value(QStringLiteral("color")).value<QColor>().name(),
                QStringLiteral("Clipboard Cache Map state color should match the main trace projection."));
    expect(!clipboardSegment.value(QStringLiteral("splitBefore")).toBool(),
           QStringLiteral("The first Clipboard segment should not draw a split-before line."));
}

void testTraceCacheMinimapClipboardSlicesStateReturnTransitions()
{
    TransactionTraceFixture fixture =
        makeCacheStateRoundTripTrace(QStringLiteral("trace_cache_minimap_clipboard_roundtrip.clogb"));

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(fixture.tracePath),
           QStringLiteral("Clipboard Cache Map round-trip fixture should load."));
    expect(window.testAddTraceCacheMinimapLane(1, 0x103F),
           QStringLiteral("Main trace Cache Map round-trip lane should be available."));
    waitForTraceCacheMinimapLane(window,
                                 0,
                                 QStringLiteral("Main trace Cache Map round-trip lane should build."));
    expectEqual(window.testTraceCacheMinimapSegmentCount(0),
                2,
                QStringLiteral("Main trace fixture should produce two UC spans separated by invalidation."));

    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Session);
    const int rowsToInsert[] = {1, 6};
    for (const int logicalRow : rowsToInsert) {
        window.testSelectLogicalRow(logicalRow);
        expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Session),
               QStringLiteral("Round-trip Clipboard source row %1 insertion should succeed.").arg(logicalRow));
    }
    QApplication::processEvents();

    expect(window.testAddClipboardCacheMinimapLane(1, 0x103F),
           QStringLiteral("Adding a Clipboard Cache Map round-trip lane should succeed."));
    waitForClipboardCacheMinimapLane(window,
                                     0,
                                     QStringLiteral("Clipboard Cache Map round-trip lane should build."));
    expectEqual(window.testClipboardCacheMinimapSegmentCount(0),
                2,
                QStringLiteral("Clipboard Cache Map should keep returned source states sliced."));

    for (int index = 0; index < 2; ++index) {
        const QVariantMap mainSegment = window.testTraceCacheMinimapSegmentAt(0, index);
        const QVariantMap clipboardSegment = window.testClipboardCacheMinimapSegmentAt(0, index);
        expectEqual(clipboardSegment.value(QStringLiteral("stateText")).toString(),
                    mainSegment.value(QStringLiteral("stateText")).toString(),
                    QStringLiteral("Clipboard round-trip state text should match the source span."));
        expectEqual(clipboardSegment.value(QStringLiteral("stateMask")).toInt(),
                    mainSegment.value(QStringLiteral("stateMask")).toInt(),
                    QStringLiteral("Clipboard round-trip state mask should match the source span."));
        expectEqual(clipboardSegment.value(QStringLiteral("startVisibleRow")).toInt(),
                    index,
                    QStringLiteral("Each sliced Clipboard segment should map to its visible row."));
        expectEqual(clipboardSegment.value(QStringLiteral("endVisibleRow")).toInt(),
                    index,
                    QStringLiteral("Each sliced Clipboard segment should map to its visible row."));
    }

    const QVariantMap first = window.testClipboardCacheMinimapSegmentAt(0, 0);
    const QVariantMap second = window.testClipboardCacheMinimapSegmentAt(0, 1);
    expectEqual(first.value(QStringLiteral("stateText")).toString(),
                second.value(QStringLiteral("stateText")).toString(),
                QStringLiteral("The fixture should return to the original cache state."));
    expect(first.value(QStringLiteral("color")).value<QColor>().name()
               == second.value(QStringLiteral("color")).value<QColor>().name(),
           QStringLiteral("Returned state should reuse the same fixed state color."));
    expect(!first.value(QStringLiteral("splitBefore")).toBool(),
           QStringLiteral("The first returned-state Clipboard segment should not expose a split-before marker."));
    expect(second.value(QStringLiteral("splitBefore")).toBool(),
           QStringLiteral("A returned-state Clipboard segment should still expose a split-before marker."));

    window.testActivateClipboardRow(second.value(QStringLiteral("startVisibleRow")).toInt());
    expectEqual(window.testSelectedLogicalRow(),
                6,
                QStringLiteral("Clipboard minimap segment activation should still jump through the Clipboard row source."));
}

void testConfigurationGuessMatchesStoredFlitLengths()
{
    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));

    CLog::Parameters declaredParameters;
    declaredParameters.SetIssue(CLog::Issue::Eb);
    expect(declaredParameters.SetNodeIdWidth(7), QStringLiteral("Failed to set declared NodeIDWidth."));
    expect(declaredParameters.SetReqAddrWidth(44), QStringLiteral("Failed to set declared ReqAddrWidth."));
    expect(declaredParameters.SetReqRSVDCWidth(0), QStringLiteral("Failed to set declared ReqRSVDCWidth."));
    expect(declaredParameters.SetDatRSVDCWidth(0), QStringLiteral("Failed to set declared DatRSVDCWidth."));
    expect(declaredParameters.SetDataWidth(128), QStringLiteral("Failed to set declared DataWidth."));
    declaredParameters.SetDataCheckPresent(false);
    declaredParameters.SetPoisonPresent(false);
    declaredParameters.SetMPAMPresent(false);

    CLog::Parameters expectedParameters;
    expectedParameters.SetIssue(CLog::Issue::Eb);
    expect(expectedParameters.SetNodeIdWidth(CHIron::Gui::ViewerConfig::nodeIdWidth),
           QStringLiteral("Failed to set expected NodeIDWidth."));
    expect(expectedParameters.SetReqAddrWidth(CHIron::Gui::ViewerConfig::reqAddrWidth),
           QStringLiteral("Failed to set expected ReqAddrWidth."));
    expect(expectedParameters.SetReqRSVDCWidth(CHIron::Gui::ViewerConfig::reqRsvdcWidth),
           QStringLiteral("Failed to set expected ReqRSVDCWidth."));
    expect(expectedParameters.SetDatRSVDCWidth(CHIron::Gui::ViewerConfig::datRsvdcWidth),
           QStringLiteral("Failed to set expected DatRSVDCWidth."));
    expect(expectedParameters.SetDataWidth(CHIron::Gui::ViewerConfig::dataWidth),
           QStringLiteral("Failed to set expected DataWidth."));
    expectedParameters.SetDataCheckPresent(CHIron::Gui::ViewerConfig::dataCheckWidth != 0);
    expectedParameters.SetPoisonPresent(CHIron::Gui::ViewerConfig::poisonWidth != 0);
    expectedParameters.SetMPAMPresent(CHIron::Gui::ViewerConfig::mpamWidth != 0);

    const std::uint8_t reqBytes = storedByteLengthForTest(CHIron::Gui::ReqFlit::WIDTH);
    const std::uint8_t rspBytes = storedByteLengthForTest(CHIron::Gui::RspFlit::WIDTH);
    const std::uint8_t datBytes = storedByteLengthForTest(CHIron::Gui::DatFlit::WIDTH);
    const std::uint8_t snpBytes = storedByteLengthForTest(CHIron::Gui::SnpFlit::WIDTH);

    const QString tracePath = tempDir.filePath(QStringLiteral("width_guess.clogb"));
    writeLengthOnlyTrace(tracePath,
                         declaredParameters,
                         {
                             makeLengthOnlyRecord(CLog::Channel::TXREQ, reqBytes),
                             makeLengthOnlyRecord(CLog::Channel::TXRSP, rspBytes),
                             makeLengthOnlyRecord(CLog::Channel::TXDAT, datBytes),
                             makeLengthOnlyRecord(CLog::Channel::TXSNP, snpBytes),
                         });

    CHIron::Gui::CLogBTraceConfigurationGuessResult result;
    CHIron::Gui::CLogBTraceLoadError error;
    expect(CHIron::Gui::CLogBTraceLoader::guessFlitConfigurations(tracePath, result, error),
           QStringLiteral("Configuration guessing should scan the synthetic CLog.B trace."));
    expect(!result.guesses.empty(),
           QStringLiteral("Configuration guessing should find at least one matching parameter set."));
    expect(result.observedByteLengths[0].size() == 1 && result.observedByteLengths[0][0] == reqBytes,
           QStringLiteral("Configuration guessing should preserve observed REQ byte lengths."));
    expect(result.observedByteLengths[2].size() == 1 && result.observedByteLengths[2][0] == datBytes,
           QStringLiteral("Configuration guessing should preserve observed DAT byte lengths."));

    const auto found = std::find_if(result.guesses.begin(),
                                    result.guesses.end(),
                                    [&](const CHIron::Gui::CLogBTraceConfigurationGuess& guess) {
                                        return sameParameters(guess.parameters, expectedParameters);
                                    });
    expect(found != result.guesses.end(),
           QStringLiteral("Configuration guessing should include the parameter set that produced the stored widths."));
}

void testAddressHexPrefixIsIgnored()
{
    CHIron::Gui::FlitTableModel model;
    model.setRows(buildFilterTestRows());

    const int totalRows = model.totalCount();
    model.setAddressFilter(QStringLiteral("0x"));

    expectEqual(model.visibleCount(),
                totalRows,
                QStringLiteral("Bare 0x address filters should behave like an empty filter."));
}

void testNumericAddressAndDbidFiltersMatchExpectedRows()
{
    CHIron::Gui::FlitTableModel model;
    model.setRows(buildFilterTestRows());

    model.setAddressFilter(QStringLiteral("0x12000"));
    expectEqual(model.visibleCount(),
                1,
                QStringLiteral("Address filter should match the canonical numeric address."));
    const CHIron::Gui::FlitRecord* addressRow = model.recordAt(0);
    expect(addressRow != nullptr, QStringLiteral("Address-filtered row should exist."));
    expectEqual(static_cast<int>(addressRow->timestamp),
                100,
                QStringLiteral("Address filter should retain the expected sample request row."));

    model.setAddressFilter(QString());
    model.setDbidFilter(QStringLiteral("12"));
    expectEqual(model.visibleCount(),
                3,
                QStringLiteral("DBID filter should match the response plus both data beats for DBID 12."));
}

void testSearchHighlightModeKeepsRowsAndMarksMatches()
{
    CHIron::Gui::FlitTableModel model;
    model.setRows(buildFilterTestRows());

    const int totalRows = model.totalCount();
    model.setSearchMode(CHIron::Gui::FlitTableModel::SearchMode::Highlight);
    model.setAddressFilter(QStringLiteral("0x12000"));

    expectEqual(model.visibleCount(),
                totalRows,
                QStringLiteral("Highlight search mode should not hide non-matching flits."));

    const QModelIndex matchingIndex = model.index(0, CHIron::Gui::FlitTableModel::OpcodeColumn);
    const QColor matchingBackground = model.data(matchingIndex, Qt::BackgroundRole).value<QColor>();
    expect(matchingBackground.isValid(),
           QStringLiteral("Highlight search mode should mark matching flits with a background color."));

    const QModelIndex nonMatchingIndex = model.index(1, CHIron::Gui::FlitTableModel::OpcodeColumn);
    const QColor nonMatchingBackground = model.data(nonMatchingIndex, Qt::BackgroundRole).value<QColor>();
    expect(!nonMatchingBackground.isValid(),
           QStringLiteral("Highlight search mode should leave non-matching unaccented rows unhighlighted."));

    model.setAddressFilter(QString());
    model.setDbidFilter(QStringLiteral("12"));
    expectEqual(model.visibleCount(),
                totalRows,
                QStringLiteral("Highlight search mode should still keep all rows visible after changing search fields."));

    const QModelIndex nonMatchingReqIndex = model.index(0, CHIron::Gui::FlitTableModel::OpcodeColumn);
    const QColor nonMatchingReqBackground = model.data(nonMatchingReqIndex, Qt::BackgroundRole).value<QColor>();
    expect(!nonMatchingReqBackground.isValid(),
           QStringLiteral("Highlight search mode should suppress other row background colors on non-matching rows."));

    const QModelIndex matchingRspIndex = model.index(1, CHIron::Gui::FlitTableModel::OpcodeColumn);
    const QColor matchingRspBackground = model.data(matchingRspIndex, Qt::BackgroundRole).value<QColor>();
    expect(matchingRspBackground.isValid(),
           QStringLiteral("Highlight search mode should still mark matching rows after changing search fields."));
    expect(model.isSearchHighlightedRow(1),
           QStringLiteral("Highlight search mode should report matching visible rows."));
    expect(!model.isSearchHighlightedRow(0),
           QStringLiteral("Highlight search mode should report non-matching visible rows."));
    expectEqual(model.findSearchHighlightedRow(-1, true),
                1,
                QStringLiteral("Forward highlight navigation should find the first highlighted row."));
    expectEqual(model.findSearchHighlightedRow(1, true),
                2,
                QStringLiteral("Forward highlight navigation should advance to the next highlighted row."));
    expectEqual(model.findSearchHighlightedRow(3, true),
                1,
                QStringLiteral("Forward highlight navigation should wrap to the first highlighted row."));
    expectEqual(model.findSearchHighlightedRow(1, false),
                3,
                QStringLiteral("Reverse highlight navigation should wrap to the last highlighted row."));
    const CHIron::Gui::FlitTableModel::SearchHighlightNavigationIndex secondMatchIndex =
        model.searchHighlightNavigationIndex(2);
    expectEqual(secondMatchIndex.offset,
                2,
                QStringLiteral("Highlight navigation index should report the selected match offset."));
    expectEqual(secondMatchIndex.total,
                3,
                QStringLiteral("Highlight navigation index should report the total match count."));
    const CHIron::Gui::FlitTableModel::SearchHighlightNavigationIndex nonMatchIndex =
        model.searchHighlightNavigationIndex(0);
    expectEqual(nonMatchIndex.offset,
                0,
                QStringLiteral("Highlight navigation index should report zero offset for a non-matching selected row."));
    expectEqual(nonMatchIndex.total,
                3,
                QStringLiteral("Highlight navigation index should still report the total match count from a non-matching selected row."));

    model.setSearchMode(CHIron::Gui::FlitTableModel::SearchMode::Filter);
    expectEqual(model.visibleCount(),
                3,
                QStringLiteral("Filtering search mode should apply the same search terms as row filters."));
}

void testTraceTableTimestampDisplayUsesThousandsSeparators()
{
    CHIron::Gui::FlitRecord record;
    record.timestamp = 1234567890;
    record.opcode = QStringLiteral("TestOp");

    CHIron::Gui::FlitTableModel model;
    model.setRows({record});

    expectEqual(model.data(model.index(0, CHIron::Gui::FlitTableModel::TimeColumn),
                           Qt::DisplayRole).toString(),
                QStringLiteral("1,234,567,890"),
                QStringLiteral("Trace table timestamp display should use thousands separators."));

    model.setDisplayMode(CHIron::Gui::FlitTableModel::TimeColumn,
                         CHIron::Gui::FlitTableModel::DisplayMode::Decimal);
    expectEqual(model.data(model.index(0, CHIron::Gui::FlitTableModel::TimeColumn),
                           Qt::DisplayRole).toString(),
                QStringLiteral("1,234,567,890"),
                QStringLiteral("Trace table decimal timestamp display should keep thousands separators."));
}

void testFlitTableDeleteUndoRedoForRowBackedRows()
{
    CHIron::Gui::FlitTableModel model;
    model.setRows(buildFilterTestRows());
    model.setEditable(true);

    expect(model.deleteRowAt(1), QStringLiteral("Deleting a visible row should succeed in editable mode."));
    expectEqual(model.totalCount(), 3, QStringLiteral("Delete should remove one row from a row-backed model."));
    expectEqual(model.recordAt(1)->opcode,
                QStringLiteral("CompData"),
                QStringLiteral("Delete should shift following rows into the removed slot."));
    expect(model.canUndo(), QStringLiteral("Delete should create an undo command."));

    model.undoEdit();
    expectEqual(model.totalCount(), 4, QStringLiteral("Undo delete should restore row count."));
    expectEqual(model.recordAt(1)->opcode,
                QStringLiteral("Comp"),
                QStringLiteral("Undo delete should restore the deleted row at its old position."));
    expect(model.canRedo(), QStringLiteral("Undo delete should create a redo command."));

    model.redoEdit();
    expectEqual(model.totalCount(), 3, QStringLiteral("Redo delete should remove the row again."));
    expectEqual(model.recordAt(1)->opcode,
                QStringLiteral("CompData"),
                QStringLiteral("Redo delete should restore the deleted-row gap."));
}

void testFlitTableTimestampInsertDeleteUndoRedoKeepsOrder()
{
    CHIron::Gui::FlitTableModel model;
    model.setRows(buildFilterTestRows());
    model.setEditable(true);

    CHIron::Gui::FlitRecord inserted = buildFilterTestRows().front();
    inserted.timestamp = 112;
    inserted.opcode = QStringLiteral("InsertedSameTimestamp");
    inserted.summary = QStringLiteral("Inserted same timestamp row");

    expect(model.insertRowsByTimestamp({inserted}),
           QStringLiteral("Timestamp insert should succeed in editable mode."));
    expectEqual(model.totalCount(), 5, QStringLiteral("Timestamp insert should add one row."));
    expectEqual(model.recordAt(2)->opcode,
                QStringLiteral("InsertedSameTimestamp"),
                QStringLiteral("Timestamp insert should place equal timestamps after existing rows."));

    model.undoEdit();
    expectEqual(model.totalCount(), 4, QStringLiteral("Undo timestamp insert should remove inserted row."));
    expectEqual(model.recordAt(2)->opcode,
                QStringLiteral("CompData"),
                QStringLiteral("Undo timestamp insert should restore original row order."));

    model.redoEdit();
    expectEqual(model.recordAt(2)->opcode,
                QStringLiteral("InsertedSameTimestamp"),
                QStringLiteral("Redo timestamp insert should restore inserted row."));

    expect(model.deleteRowAt(2), QStringLiteral("Deleting an inserted row should succeed."));
    expectEqual(model.totalCount(), 4, QStringLiteral("Deleting inserted row should remove it from view."));
    expectEqual(model.recordAt(2)->opcode,
                QStringLiteral("CompData"),
                QStringLiteral("Deleting inserted row should reveal following original row."));

    model.undoEdit();
    expectEqual(model.totalCount(), 5, QStringLiteral("Undo inserted-row delete should restore row count."));
    expectEqual(model.recordAt(2)->opcode,
                QStringLiteral("InsertedSameTimestamp"),
                QStringLiteral("Undo inserted-row delete should restore inserted row."));
}

void testFlitTableSessionBackedDeleteUndoStreamsRows()
{
    TransactionTraceFixture fixture = makeIndexedReadNoSnpTrace(QStringLiteral("session_backed_delete.clogb"), 1);
    CHIron::Gui::CLogBTraceLoadError error;
    std::shared_ptr<CHIron::Gui::TraceSession> session;
    expect(CHIron::Gui::TraceSession::open(fixture.tracePath, session, error),
           error.summary.isEmpty()
               ? QStringLiteral("Trace session should open for session-backed delete test.")
               : error.summary);

    CHIron::Gui::FlitTableModel model;
    model.setTraceSession(session);
    model.setEditable(true);
    expectEqual(model.totalCount(), 3, QStringLiteral("Session-backed model should start with all rows."));

    expect(model.deleteRowAt(1), QStringLiteral("Deleting a session-backed original row should succeed."));
    expectEqual(model.totalCount(), 2, QStringLiteral("Session-backed delete should remove one logical row."));

    int streamedRows = 0;
    expect(model.forEachSourceRowInOrder(
               [&streamedRows](const CHIron::Gui::FlitRecord&, int) {
                   ++streamedRows;
                   return true;
               },
               error),
           error.summary.isEmpty()
               ? QStringLiteral("Streaming session-backed rows after delete should succeed.")
               : error.summary);
    expectEqual(streamedRows, 2, QStringLiteral("Streaming should skip the staged deleted row."));

    model.undoEdit();
    expectEqual(model.totalCount(), 3, QStringLiteral("Undo session-backed delete should restore row count."));
    streamedRows = 0;
    expect(model.forEachSourceRowInOrder(
               [&streamedRows](const CHIron::Gui::FlitRecord&, int) {
                   ++streamedRows;
                   return true;
               },
               error),
           error.summary.isEmpty()
               ? QStringLiteral("Streaming session-backed rows after undo should succeed.")
               : error.summary);
    expectEqual(streamedRows, 3, QStringLiteral("Streaming after undo should include the restored row."));
}

void testMainWindowTraceToolbarFoldsAndUnfolds()
{
    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    const int expandedHeight = window.testTraceToolbarHeight();
    expectGreater(expandedHeight,
                  0,
                  QStringLiteral("Flits toolbar should have a visible expanded height."));
    expect(!window.testTraceToolbarFolded(),
           QStringLiteral("Flits toolbar should start unfolded."));
    expect(!window.testTraceToolbarFoldedControlsHidden(),
           QStringLiteral("Flits toolbar controls should be visible while unfolded."));
    expect(window.testTraceToolbarSummaryVisible(),
           QStringLiteral("Flits toolbar summary should be visible while unfolded."));
    expect(window.testTraceToolbarFoldButtonText().isEmpty(),
           QStringLiteral("Flits toolbar fold button should be symbol-only."));
    const QVariantMap expandedButtonGeometry = window.testTraceToolbarFoldButtonGeometry();
    expect(expandedButtonGeometry.value(QStringLiteral("right")).toInt()
               >= expandedButtonGeometry.value(QStringLiteral("toolbarWidth")).toInt() - 32,
           QStringLiteral("Flits toolbar fold button should sit near the right edge."));
    expect(expandedButtonGeometry.value(QStringLiteral("bottom")).toInt()
               >= expandedButtonGeometry.value(QStringLiteral("toolbarHeight")).toInt() - 24,
           QStringLiteral("Flits toolbar fold button should sit in the bottom Search row."));

    window.testSetTraceToolbarFolded(true);
    QApplication::processEvents();

    expect(window.testTraceToolbarFolded(),
           QStringLiteral("Flits toolbar fold state should become active."));
    expect(window.testTraceToolbarFoldedControlsHidden(),
           QStringLiteral("Flits toolbar foldable controls should be hidden while folded."));
    expect(window.testTraceToolbarSummaryVisible(),
           QStringLiteral("Flits toolbar summary should stay visible while folded."));
    expect(window.testTraceToolbarHeight() < expandedHeight,
           QStringLiteral("Folded Flits toolbar should use less vertical space."));
    const QVariantMap foldedButtonGeometry = window.testTraceToolbarFoldButtonGeometry();
    expect(foldedButtonGeometry.value(QStringLiteral("right")).toInt()
               >= foldedButtonGeometry.value(QStringLiteral("toolbarWidth")).toInt() - 32,
           QStringLiteral("Folded Flits toolbar fold button should stay near the right edge."));
    expect(foldedButtonGeometry.value(QStringLiteral("y")).toInt() <= 8,
           QStringLiteral("Folded Flits toolbar fold button should move onto the Summary row."));

    window.testSetTraceToolbarFolded(false);
    QApplication::processEvents();

    expect(!window.testTraceToolbarFolded(),
           QStringLiteral("Flits toolbar fold state should clear after unfolding."));
    expect(!window.testTraceToolbarFoldedControlsHidden(),
           QStringLiteral("Flits toolbar controls should be visible after unfolding."));
    expect(window.testTraceToolbarSummaryVisible(),
           QStringLiteral("Flits toolbar summary should remain visible after unfolding."));
    expect(window.testTraceToolbarHeight() >= expandedHeight,
           QStringLiteral("Unfolded Flits toolbar should restore its expanded height."));
}

void testMainWindowKeepsIndependentTraceSessions()
{
    std::vector<CHIron::Gui::FlitRecord> firstRows = buildFilterTestRows();
    std::vector<CHIron::Gui::FlitRecord> secondRows = buildFilterTestRows();
    for (CHIron::Gui::FlitRecord& row : secondRows) {
        row.opcode = QStringLiteral("SecondOnly");
        row.summary = QStringLiteral("Second session row");
    }

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testApplyTraceRows(std::move(firstRows), QStringLiteral("trace.clogb")),
           QStringLiteral("Main window should accept the first row-backed test session."));
    expectEqual(window.testSessionCount(),
                1,
                QStringLiteral("Opening one trace should create one session."));
    expectEqual(window.testActiveSessionLabel(),
                QStringLiteral("trace.clogb"),
                QStringLiteral("The first session label should become active."));
    expectEqual(window.testVisibleTraceRowCount(),
                4,
                QStringLiteral("The first session should expose its full row set."));

    window.testSetOpcodeFilter(QStringLiteral("Read"));
    QApplication::processEvents();
    expectEqual(window.testVisibleTraceRowCount(),
                1,
                QStringLiteral("Filtering the first session should affect the active table."));

    expect(window.testApplyTraceRows(std::move(secondRows), QStringLiteral("trace.clogb")),
           QStringLiteral("Main window should accept a second row-backed test session."));
    expectEqual(window.testSessionCount(),
                2,
                QStringLiteral("Opening a second trace should create a second session."));
    expectEqual(window.testActiveSessionIndex(),
                1,
                QStringLiteral("The newest opened trace should become active."));
    expectEqual(window.testActiveSessionLabel(),
                QStringLiteral("trace.clogb (2)"),
                QStringLiteral("Duplicate session labels should be made unique."));
    expectEqual(window.testVisibleTraceRowCount(),
                4,
                QStringLiteral("The second session should not inherit the first session filter."));
    expectEqual(window.testOpcodeFilter(),
                QString(),
                QStringLiteral("The second session should start with an empty opcode filter."));

    expect(window.testSwitchToSession(0),
           QStringLiteral("Switching back to the first session should succeed."));
    QApplication::processEvents();
    expectEqual(window.testActiveSessionLabel(),
                QStringLiteral("trace.clogb"),
                QStringLiteral("The first session label should be restored after switching."));
    expectEqual(window.testOpcodeFilter(),
                QStringLiteral("Read"),
                QStringLiteral("The first session opcode filter should be preserved."));
    expectEqual(window.testVisibleTraceRowCount(),
                1,
                QStringLiteral("The first session filtered row count should be preserved."));

    expect(window.testSwitchToSession(1),
           QStringLiteral("Switching back to the second session should succeed."));
    QApplication::processEvents();
    expectEqual(window.testOpcodeFilter(),
                QString(),
                QStringLiteral("The second session should still have no opcode filter."));
    expectEqual(window.testVisibleTraceRowCount(),
                4,
                QStringLiteral("The second session should keep its independent visible row count."));
}

void testClipboardWidgetOrderingDuplicatesFilteringAndDeletion()
{
    std::vector<CHIron::Gui::FlitRecord> rows = buildFilterTestRows();
    rows[0].timestamp = 300;
    rows[1].timestamp = 100;
    rows[2].timestamp = 200;
    rows[3].timestamp = 200;

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testApplyTraceRows(std::move(rows), QStringLiteral("clipboard_order.clogb")),
           QStringLiteral("Clipboard ordering test session should open."));
    expect(window.testClipboardToolbarFolded(),
           QStringLiteral("Clipboard toolbar should start folded."));
    expect(window.testClipboardReadOnly(),
           QStringLiteral("Clipboard should start in read-only mode."));

    window.testSelectLogicalRow(0);
    expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Session),
           QStringLiteral("Inserting the first selected row into the session Clipboard should succeed."));
    window.testSelectLogicalRow(1);
    expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Session),
           QStringLiteral("Inserting the second selected row into the session Clipboard should succeed."));
    window.testSelectLogicalRow(2);
    expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Session),
           QStringLiteral("Inserting the third selected row into the session Clipboard should succeed."));
    window.testSelectLogicalRow(3);
    expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Session),
           QStringLiteral("Inserting the fourth selected row into the session Clipboard should succeed."));

    expectEqual(window.testClipboardRowCount(),
                4,
                QStringLiteral("Session Clipboard should contain all inserted source rows."));
    expect(window.testClipboardTimestampAt(0) == 300,
           QStringLiteral("Unedited Clipboard rows should preserve main-trace source order instead of timestamp sorting."));
    expect(window.testClipboardTimestampAt(1) == 100,
           QStringLiteral("Second Clipboard row should remain the second source row."));
    expect(window.testClipboardTimestampAt(2) == 200,
           QStringLiteral("Third Clipboard row should remain the third source row."));
    expect(window.testClipboardTimestampAt(3) == 200,
           QStringLiteral("Fourth Clipboard row should remain the fourth source row."));
    expectEqual(window.testClipboardOpcodeAt(2),
                QStringLiteral("CompData"),
                QStringLiteral("Main-trace subset ordering should keep the first DAT source row next."));
    expectEqual(window.testClipboardOpcodeAt(3),
                QStringLiteral("CompData"),
                QStringLiteral("Main-trace subset ordering should keep the later DAT source row last."));

    window.testSelectLogicalRow(2);
    expect(!window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Session),
           QStringLiteral("Re-inserting the same source row should be skipped as a duplicate."));
    expectEqual(window.testClipboardRowCount(),
                4,
                QStringLiteral("Duplicate source rows should not increase Clipboard row count."));

    window.testSetClipboardOpcodeFilter(QStringLiteral("Read"));
    QApplication::processEvents();
    expectEqual(window.testClipboardVisibleRowCount(),
                1,
                QStringLiteral("Clipboard opcode filtering should reuse flit table filtering."));
    window.testSetClipboardOpcodeFilter(QString());
    QApplication::processEvents();
    expectEqual(window.testClipboardVisibleRowCount(),
                4,
                QStringLiteral("Clearing the Clipboard filter should restore all Clipboard rows."));

    const int mainTraceRowsBeforeDelete = window.testVisibleTraceRowCount();
    expect(window.testDeleteClipboardRow(0),
           QStringLiteral("Deleting a Clipboard row should succeed."));
    expectEqual(window.testClipboardRowCount(),
                3,
                QStringLiteral("Deleting from Clipboard should reduce Clipboard row count."));
    expectEqual(window.testVisibleTraceRowCount(),
                mainTraceRowsBeforeDelete,
                QStringLiteral("Deleting from Clipboard should not alter the main trace."));
}

void testClipboardSingleClickHighlightsRelatedTransactionRows()
{
    std::vector<CHIron::Gui::FlitRecord> rows = buildFilterTestRows();
    rows[0].transactionGroupKey = QStringLiteral("xaction|1");
    rows[1].transactionGroupKey = QStringLiteral("xaction|1");
    rows[2].transactionGroupKey = QStringLiteral("xaction|1");
    rows[3].transactionGroupKey = QStringLiteral("xaction|2");

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testApplyTraceRows(std::move(rows), QStringLiteral("clipboard_highlight.clogb")),
           QStringLiteral("Clipboard transaction highlight fixture should open."));
    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Session);
    for (int row = 0; row < 3; ++row) {
        window.testSelectLogicalRow(row);
        expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Session),
               QStringLiteral("Clipboard transaction highlight row insertion should succeed."));
    }

    expect(window.testClickClipboardRow(1),
           QStringLiteral("Single-clicking a Clipboard row should be delivered to the Clipboard table."));
    QApplication::processEvents();
    expect(window.testClipboardRowTransactionHighlighted(0),
           QStringLiteral("Clipboard click should highlight an earlier related transaction row."));
    expect(window.testClipboardRowTransactionHighlighted(1),
           QStringLiteral("Clipboard click should highlight the clicked row."));
    expect(window.testClipboardRowTransactionHighlighted(2),
           QStringLiteral("Clipboard click should highlight later related transaction rows."));
    expect(!window.testClipboardRowTransactionHighlighted(3),
           QStringLiteral("Clipboard click should leave unrelated transaction rows unhighlighted."));
}

void testClipboardInsertSelectedIndexedTransaction()
{
    TransactionTraceFixture fixture =
        makeIndexedReadNoSnpAddressPatternTrace(QStringLiteral("clipboard_insert_xaction.clogb"),
                                                {0x1000, 0x1040});

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(fixture.tracePath),
           QStringLiteral("Selected transaction Clipboard insertion trace should load."));
    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Session);
    window.testSelectLogicalRow(0);
    expect(window.testInsertSelectedXactionToClipboard(CHIron::Gui::ClipboardScope::Session),
           QStringLiteral("Selected transaction insertion should add the transaction rows."));
    expectEqual(window.testClipboardRowCount(),
                3,
                QStringLiteral("Selected transaction insertion should add exactly one three-flit transaction."));
    expectEqual(window.testClipboardOpcodeAt(0),
                QStringLiteral("ReadNoSnp"),
                QStringLiteral("Inserted transaction should start with its request flit."));
    expectEqual(window.testClipboardOpcodeAt(1),
                QStringLiteral("CompData"),
                QStringLiteral("Inserted transaction should include its first data beat."));
    expectEqual(window.testClipboardOpcodeAt(2),
                QStringLiteral("CompData"),
                QStringLiteral("Inserted transaction should include its second data beat."));
}

void testClipboardInsertTransactionsWithSameAddressModes()
{
    TransactionTraceFixture fixture =
        makeIndexedReadNoSnpAddressPatternTrace(QStringLiteral("clipboard_insert_address.clogb"),
                                                {0x1000, 0x1040, 0x103F, 0x1080, 0x1008});

    {
        CHIron::Gui::MainWindow window;
        window.resize(1200, 720);
        window.show();
        QApplication::processEvents();
        expect(window.testLoadTraceFile(fixture.tracePath),
               QStringLiteral("All-address Clipboard insertion trace should load."));
        window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Session);
        window.testSelectLogicalRow(6);
        expect(window.testInsertAllXactionsWithSelectedAddressToClipboard(CHIron::Gui::ClipboardScope::Session),
               QStringLiteral("All same-address insertion should add matching transactions."));
        expect(window.testClipboardInsertProgressVisible(),
               QStringLiteral("Async same-address insertion should expose status-bar progress."));
        waitForClipboardAddressInsert(window,
                                      QStringLiteral("All same-address insertion should finish in the background."));
        expectEqual(window.testClipboardRowCount(),
                    9,
                    QStringLiteral("All same-address insertion should add three matching transactions."));
        expect(!window.testClipboardCLogBSaveAvailable(),
               QStringLiteral("Fast address-batch Clipboard rows should not be immediately CLog.B-saveable."));
        expect(window.testInsertAllXactionsWithSelectedAddressToClipboard(CHIron::Gui::ClipboardScope::Session),
               QStringLiteral("Repeating all same-address insertion should still run and skip duplicate source rows."));
        waitForClipboardAddressInsert(window,
                                      QStringLiteral("Duplicate all-address insertion should finish in the background."));
        expectEqual(window.testClipboardRowCount(),
                    9,
                    QStringLiteral("Duplicate same-address insertion should not grow the Clipboard."));
    }

    {
        CHIron::Gui::MainWindow window;
        window.resize(1200, 720);
        window.show();
        QApplication::processEvents();
        expect(window.testLoadTraceFile(fixture.tracePath),
               QStringLiteral("Later-address Clipboard insertion trace should load."));
        window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Session);
        window.testSelectLogicalRow(6);
        expect(window.testInsertLaterXactionsWithSelectedAddressToClipboard(CHIron::Gui::ClipboardScope::Session),
               QStringLiteral("Later same-address insertion should add later matching transactions."));
        waitForClipboardAddressInsert(window,
                                      QStringLiteral("Later same-address insertion should finish in the background."));
        expectEqual(window.testClipboardRowCount(),
                    3,
                    QStringLiteral("Later same-address insertion should include only one later matching transaction."));
    }

    {
        CHIron::Gui::MainWindow window;
        window.resize(1200, 720);
        window.show();
        QApplication::processEvents();
        expect(window.testLoadTraceFile(fixture.tracePath),
               QStringLiteral("This-and-later-address Clipboard insertion trace should load."));
        window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Global);
        window.testSelectLogicalRow(6);
        expect(window.testInsertThisAndLaterXactionsWithSelectedAddressToClipboard(CHIron::Gui::ClipboardScope::Global),
               QStringLiteral("This-and-later same-address insertion should support Global Clipboard."));
        waitForClipboardAddressInsert(window,
                                      QStringLiteral("This-and-later same-address insertion should finish in the background."));
        expectEqual(window.testClipboardRowCount(),
                    6,
                    QStringLiteral("This-and-later same-address insertion should include selected and later matching transactions."));
    }
}

void testClipboardInsertTransactionsWithSameAddressSparseNonmatches()
{
    std::vector<std::uint64_t> addresses;
    addresses.reserve(96);
    for (int index = 0; index < 96; ++index) {
        addresses.push_back(index % 24 == 0
                                ? 0x4000 + static_cast<std::uint64_t>(index % 3) * 8
                                : 0x8000 + static_cast<std::uint64_t>(index) * 0x40);
    }
    TransactionTraceFixture fixture =
        makeIndexedReadNoSnpAddressPatternTrace(QStringLiteral("clipboard_sparse_address.clogb"),
                                                addresses);

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(fixture.tracePath),
           QStringLiteral("Sparse same-address Clipboard insertion trace should load."));
    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Session);
    window.testSelectLogicalRow(0);
    expect(window.testInsertAllXactionsWithSelectedAddressToClipboard(CHIron::Gui::ClipboardScope::Session),
           QStringLiteral("Sparse same-address insertion should start."));
    waitForClipboardAddressInsert(window,
                                  QStringLiteral("Sparse same-address insertion should finish in the background."));

    expectEqual(window.testClipboardRowCount(),
                12,
                QStringLiteral("Sparse same-address insertion should include only the four same-line transactions."));
    expectEqual(window.testClipboardTxnIdAt(0),
                QStringLiteral("0"),
                QStringLiteral("Sparse insertion should preserve the first matching transaction."));
    expectEqual(window.testClipboardTxnIdAt(3),
                QStringLiteral("24"),
                QStringLiteral("Sparse insertion should preserve the second matching transaction."));
    expectEqual(window.testClipboardTxnIdAt(6),
                QStringLiteral("48"),
                QStringLiteral("Sparse insertion should preserve the third matching transaction."));
    expectEqual(window.testClipboardTxnIdAt(9),
                QStringLiteral("72"),
                QStringLiteral("Sparse insertion should preserve the fourth matching transaction."));
}

void testClipboardBatchInsertionSkipsExistingRowsWithoutReordering()
{
    TransactionTraceFixture fixture =
        makeIndexedReadNoSnpAddressPatternTrace(QStringLiteral("clipboard_batch_duplicate_skip.clogb"),
                                                {0x1000, 0x103F, 0x1008});

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(fixture.tracePath),
           QStringLiteral("Clipboard duplicate skip trace should load."));
    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Session);

    window.testSelectLogicalRow(0);
    expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Session),
           QStringLiteral("Duplicate skip setup should insert the first request flit."));
    window.testSelectLogicalRow(3);
    expect(window.testInsertAllXactionsWithSelectedAddressToClipboard(CHIron::Gui::ClipboardScope::Session),
           QStringLiteral("Duplicate-aware batch insertion should start."));
    waitForClipboardAddressInsert(window,
                                  QStringLiteral("Duplicate-aware batch insertion should finish in the background."));

    expectEqual(window.testClipboardRowCount(),
                9,
                QStringLiteral("Duplicate-aware batch insertion should add the remaining rows only once."));
    expectEqual(window.testClipboardTxnIdAt(0),
                QStringLiteral("0"),
                QStringLiteral("Existing Clipboard row should stay at the front."));
    expectEqual(window.testClipboardOpcodeAt(0),
                QStringLiteral("ReadNoSnp"),
                QStringLiteral("Existing Clipboard row should remain unchanged."));
    expectEqual(window.testClipboardOpcodeAt(1),
                QStringLiteral("CompData"),
                QStringLiteral("Batch insertion should append the first missing row after the existing duplicate."));
    expectEqual(window.testClipboardTxnIdAt(3),
                QStringLiteral("1"),
                QStringLiteral("Batch insertion should keep later transactions in logical order."));
    expectEqual(window.testClipboardTxnIdAt(6),
                QStringLiteral("2"),
                QStringLiteral("Batch insertion should keep the final transaction in logical order."));
}

void testClipboardBatchInsertionPreservesLogicalOrder()
{
    TransactionTraceFixture fixture =
        makeIndexedReadNoSnpAddressPatternTrace(QStringLiteral("clipboard_batch_order.clogb"),
                                                {0x1000, 0x103F, 0x1008},
                                                true);

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(fixture.tracePath),
           QStringLiteral("Clipboard batch ordering trace should load."));
    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Session);

    window.testSelectLogicalRow(6);
    expect(window.testInsertAllXactionsWithSelectedAddressToClipboard(CHIron::Gui::ClipboardScope::Session),
           QStringLiteral("Batch insertion should start for ordering regression."));
    waitForClipboardAddressInsert(window,
                                  QStringLiteral("Batch ordering insertion should finish in the background."));

    expectEqual(window.testClipboardRowCount(),
                9,
                QStringLiteral("Batch ordering insertion should add all same-line transactions."));
    expectEqual(window.testClipboardOpcodeAt(0),
                QStringLiteral("ReadNoSnp"),
                QStringLiteral("Batch order should start at the earliest logical request."));
    expectEqual(window.testClipboardTxnIdAt(0),
                QStringLiteral("0"),
                QStringLiteral("Batch insertion should preserve the first logical transaction."));
    expectEqual(window.testClipboardTxnIdAt(3),
                QStringLiteral("1"),
                QStringLiteral("Batch insertion should preserve the second logical transaction."));
    expectEqual(window.testClipboardTxnIdAt(6),
                QStringLiteral("2"),
                QStringLiteral("Batch insertion should preserve the third logical transaction."));
    expectEqual(window.testClipboardOpcodeAt(3),
                QStringLiteral("ReadNoSnp"),
                QStringLiteral("Batch order should keep the second transaction contiguous."));
    expectEqual(window.testClipboardOpcodeAt(6),
                QStringLiteral("ReadNoSnp"),
                QStringLiteral("Batch order should keep the third transaction contiguous."));
}

void testClipboardBatchInsertionMergesExistingSubsetByLogicalRow()
{
    TransactionTraceFixture fixture =
        makeIndexedReadNoSnpAddressPatternTrace(QStringLiteral("clipboard_batch_merge_subset.clogb"),
                                                {0x1000, 0x103F, 0x1008},
                                                true);

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(fixture.tracePath),
           QStringLiteral("Clipboard batch subset merge trace should load."));
    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Session);

    window.testSelectLogicalRow(6);
    expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Session),
           QStringLiteral("Subset merge setup should insert a later source row first."));
    window.testSelectLogicalRow(0);
    expect(window.testInsertAllXactionsWithSelectedAddressToClipboard(CHIron::Gui::ClipboardScope::Session),
           QStringLiteral("Subset merge batch insertion should start."));
    waitForClipboardAddressInsert(window,
                                  QStringLiteral("Subset merge batch insertion should finish in the background."));

    expectEqual(window.testClipboardRowCount(),
                9,
                QStringLiteral("Subset merge insertion should contain all same-line rows exactly once."));
    expectEqual(window.testClipboardTxnIdAt(0),
                QStringLiteral("0"),
                QStringLiteral("Subset merge should restore earliest transaction to the front."));
    expectEqual(window.testClipboardTxnIdAt(3),
                QStringLiteral("1"),
                QStringLiteral("Subset merge should place the middle transaction next."));
    expectEqual(window.testClipboardTxnIdAt(6),
                QStringLiteral("2"),
                QStringLiteral("Subset merge should keep the originally inserted later transaction in source order."));
}

void testClipboardAddressBatchCancelControlVisible()
{
    TransactionTraceFixture fixture =
        makeIndexedReadNoSnpAddressPatternTrace(QStringLiteral("clipboard_batch_cancel_button.clogb"),
                                                {0x1000, 0x103F, 0x1008, 0x1040, 0x1004});

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(fixture.tracePath),
           QStringLiteral("Clipboard cancel control fixture should load."));
    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Session);
    window.testSelectLogicalRow(0);
    expect(window.testInsertAllXactionsWithSelectedAddressToClipboard(CHIron::Gui::ClipboardScope::Session),
           QStringLiteral("Clipboard address batch should start for cancel control test."));
    expect(window.testClipboardInsertProgressVisible(),
           QStringLiteral("Clipboard address batch progress should be visible."));
    expect(window.testClipboardInsertCancelVisible(),
           QStringLiteral("Clipboard address batch cancel button should be visible."));
    expect(window.testCancelClipboardXactionAddressInsert(),
           QStringLiteral("Clipboard address batch cancel request should be accepted."));
    waitForClipboardAddressInsert(window,
                                  QStringLiteral("Clipboard address batch cancel should settle."));
    expect(!window.testClipboardInsertProgressVisible(),
           QStringLiteral("Clipboard address batch progress should hide after cancellation settles."));
}

void testClipboardSameAddressInsertionKeepsEventLoopResponsive()
{
    std::vector<std::uint64_t> addresses;
    addresses.reserve(2048);
    for (int index = 0; index < 2048; ++index) {
        addresses.push_back(index % 2 == 0
                                ? 0x1000 + static_cast<std::uint64_t>(index % 8) * 8
                                : 0x8000 + static_cast<std::uint64_t>(index) * 0x40);
    }
    TransactionTraceFixture fixture =
        makeIndexedReadNoSnpAddressPatternTrace(QStringLiteral("clipboard_same_address_responsive.clogb"),
                                                addresses);

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(fixture.tracePath),
           QStringLiteral("Same-address responsiveness fixture should load."));
    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Session);
    window.testSelectLogicalRow(0);

    int heartbeatCount = 0;
    qint64 maxHeartbeatGapMs = 0;
    QElapsedTimer heartbeatTimer;
    heartbeatTimer.start();
    qint64 lastHeartbeatMs = heartbeatTimer.elapsed();
    QTimer heartbeat;
    heartbeat.setInterval(10);
    QObject::connect(&heartbeat, &QTimer::timeout, [&]() {
        const qint64 nowMs = heartbeatTimer.elapsed();
        maxHeartbeatGapMs = std::max(maxHeartbeatGapMs, nowMs - lastHeartbeatMs);
        lastHeartbeatMs = nowMs;
        ++heartbeatCount;
    });
    heartbeat.start();

    expect(window.testInsertAllXactionsWithSelectedAddressToClipboard(CHIron::Gui::ClipboardScope::Session),
           QStringLiteral("Same-address responsiveness insertion should start."));
    const bool completed = waitForCondition([&window]() {
        return !window.testClipboardXactionAddressInsertActive();
    }, 15000);
    heartbeat.stop();

    expect(completed, QStringLiteral("Same-address responsiveness insertion should complete."));
    expect(heartbeatCount > 0,
           QStringLiteral("Same-address responsiveness insertion should keep processing timer events."));
    expect(maxHeartbeatGapMs <= 100,
           QStringLiteral("Same-address responsiveness insertion should not block the Qt event loop for long stretches (max gap=%1 ms).")
               .arg(maxHeartbeatGapMs));
    expect(window.testClipboardRowCount() > 0,
           QStringLiteral("Same-address responsiveness insertion should publish matching Clipboard rows."));
}

void testClipboardInsertionSortsOnlyEditedRows()
{
    TransactionTraceFixture fixture =
        makeIndexedReadNoSnpAddressPatternTrace(QStringLiteral("clipboard_edited_local_sort.clogb"),
                                                {0x1000, 0x1040});

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(fixture.tracePath),
           QStringLiteral("Clipboard edited local sort fixture should open."));
    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Session);

    for (int row = 0; row < 3; ++row) {
        window.testSelectLogicalRow(row);
        expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Session),
               QStringLiteral("Edited local sort setup insertion should succeed."));
    }
    window.testSetClipboardReadOnly(false);
    expect(window.testEditClipboardTimestampAt(1, 1000),
           QStringLiteral("Edited local sort should edit one Clipboard row."));
    expect(window.testClipboardEntryModified(1),
           QStringLiteral("Edited local sort should mark the edited row."));

    window.testSelectLogicalRow(3);
    expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Session),
           QStringLiteral("Edited local sort should allow another insertion."));

    expectEqual(static_cast<int>(window.testClipboardTimestampAt(0)),
                1,
                QStringLiteral("Unedited first row should stay in source order."));
    expectEqual(static_cast<int>(window.testClipboardTimestampAt(1)),
                1000,
                QStringLiteral("Edited row should remain in the edited-row slot after localized sorting."));
    expectEqual(static_cast<int>(window.testClipboardTimestampAt(2)),
                6,
                QStringLiteral("Unedited third row should stay in source order."));
    expectEqual(static_cast<int>(window.testClipboardTimestampAt(3)),
                7,
                QStringLiteral("New unedited row should be merged into the unedited source-order subset."));
}

void testClipboardVisiblePageMaterializesVisibleColumns()
{
    TransactionTraceFixture fixture =
        makeClipboardMaterializationTrace(QStringLiteral("clipboard_visible_page_materialization.clogb"),
                                          1030);

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(fixture.tracePath),
           QStringLiteral("Clipboard materialization fixture should load."));
    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Session);

    for (const int logicalRow : {0, 3, 6, 1026, 1027}) {
        window.testSelectLogicalRow(logicalRow);
        expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Session),
               QStringLiteral("Clipboard materialization setup insertion for logical row %1 should succeed.")
                   .arg(logicalRow));
    }
    expectEqual(window.testClipboardRowCount(),
                5,
                QStringLiteral("Clipboard materialization setup should contain the selected rows."));

    const QString retryField = QStringLiteral("AllowRetry");
    const QString expCompAckField = QStringLiteral("ExpCompAck");
    const QString traceTagField = QStringLiteral("TraceTag");
    for (const QString& fieldName : {retryField, expCompAckField, traceTagField}) {
        expect(window.testSetClipboardFieldColumnVisible(fieldName, true),
               QStringLiteral("Clipboard materialization test should expose the %1 column.")
                   .arg(fieldName));
    }
    expectEqual(window.testClipboardFieldValueAt(0, retryField),
                QStringLiteral("0"),
                QStringLiteral("Materialization setup should decode AllowRetry for source row 0."));
    expectEqual(window.testClipboardFieldValueAt(1, retryField),
                QStringLiteral("1"),
                QStringLiteral("Materialization setup should decode AllowRetry for source row 3."));
    expectEqual(window.testClipboardFieldValueAt(2, retryField),
                QStringLiteral("0"),
                QStringLiteral("Materialization setup should decode AllowRetry for source row 6."));
    expectEqual(window.testClipboardFieldValueAt(4, retryField),
                QStringLiteral("1"),
                QStringLiteral("Materialization setup should decode AllowRetry for source row 1027."));
    expectEqual(window.testClipboardFieldValueAt(0, expCompAckField),
                QStringLiteral("1"),
                QStringLiteral("Materialization setup should decode ExpCompAck for source row 0."));
    expectEqual(window.testClipboardFieldValueAt(1, traceTagField),
                QStringLiteral("1"),
                QStringLiteral("Materialization setup should decode later TraceTag for source row 3."));

    expect(window.testRemoveClipboardFieldAt(0, retryField),
           QStringLiteral("Clipboard materialization should remove AllowRetry from first same-page row."));
    expect(window.testRemoveClipboardFieldAt(0, expCompAckField),
           QStringLiteral("Clipboard materialization should remove ExpCompAck from first same-page row."));
    expect(window.testRemoveClipboardFieldAt(0, traceTagField),
           QStringLiteral("Clipboard materialization should remove TraceTag from first same-page row."));
    expect(window.testRemoveClipboardFieldAt(1, retryField),
           QStringLiteral("Clipboard materialization should remove AllowRetry from second same-page row."));
    expect(window.testRemoveClipboardFieldAt(1, expCompAckField),
           QStringLiteral("Clipboard materialization should remove ExpCompAck from second same-page row."));
    expect(window.testRemoveClipboardFieldAt(1, traceTagField),
           QStringLiteral("Clipboard materialization should remove TraceTag from second same-page row."));
    expect(window.testSetClipboardFieldValueAt(2, retryField, QStringLiteral("edited")),
           QStringLiteral("Clipboard materialization should edit the modified same-page row's AllowRetry."));
    expect(window.testRemoveClipboardFieldAt(2, traceTagField),
           QStringLiteral("Clipboard materialization should remove a missing later field from modified same-page row."));
    expect(window.testRemoveClipboardFieldAt(3, retryField),
           QStringLiteral("Clipboard materialization should remove AllowRetry from the other-page row."));
    expect(window.testRemoveClipboardFieldAt(3, expCompAckField),
           QStringLiteral("Clipboard materialization should remove ExpCompAck from the other-page row."));
    expect(window.testRemoveClipboardFieldAt(3, traceTagField),
           QStringLiteral("Clipboard materialization should remove TraceTag from the other-page row."));

    expectEqual(window.testClipboardFieldValueAt(0, retryField),
                QString(),
                QStringLiteral("First same-page row should initially lack AllowRetry."));
    expectEqual(window.testClipboardFieldValueAt(0, expCompAckField),
                QString(),
                QStringLiteral("First same-page row should initially lack ExpCompAck."));
    expectEqual(window.testClipboardFieldValueAt(0, traceTagField),
                QString(),
                QStringLiteral("First same-page row should initially lack TraceTag."));
    expectEqual(window.testClipboardFieldValueAt(1, retryField),
                QString(),
                QStringLiteral("Second same-page row should initially lack AllowRetry."));
    expectEqual(window.testClipboardFieldValueAt(1, expCompAckField),
                QString(),
                QStringLiteral("Second same-page row should initially lack ExpCompAck."));
    expectEqual(window.testClipboardFieldValueAt(1, traceTagField),
                QString(),
                QStringLiteral("Second same-page row should initially lack TraceTag."));
    expectEqual(window.testClipboardFieldValueAt(2, retryField),
                QStringLiteral("edited"),
                QStringLiteral("Modified same-page row should carry its edited AllowRetry value before hydration."));
    expectEqual(window.testClipboardFieldValueAt(2, traceTagField),
                QString(),
                QStringLiteral("Modified same-page row should initially lack the later TraceTag value."));
    expectEqual(window.testClipboardFieldValueAt(3, retryField),
                QString(),
                QStringLiteral("Other-page row should initially lack AllowRetry."));
    expectEqual(window.testClipboardFieldValueAt(3, expCompAckField),
                QString(),
                QStringLiteral("Other-page row should initially lack ExpCompAck."));
    expectEqual(window.testClipboardFieldValueAt(3, traceTagField),
                QString(),
                QStringLiteral("Other-page row should initially lack TraceTag."));
    expect(window.testClipboardEntryModified(2),
           QStringLiteral("Edited AllowRetry row should be marked modified before hydration."));

    expect(window.testClickClipboardRow(0),
           QStringLiteral("Clipboard materialization should select the first same-page row as anchor."));
    window.testMaterializeClipboardVisiblePage();
    QApplication::processEvents();

    expectEqual(window.testClipboardFieldValueAt(0, retryField),
                QStringLiteral("0"),
                QStringLiteral("First same-page row should hydrate AllowRetry from its source page."));
    expectEqual(window.testClipboardFieldValueAt(1, retryField),
                QStringLiteral("1"),
                QStringLiteral("Second same-page row should hydrate AllowRetry from the same source page."));
    expectEqual(window.testClipboardFieldValueAt(0, expCompAckField),
                QStringLiteral("1"),
                QStringLiteral("First same-page row should hydrate later ExpCompAck from its source page."));
    expectEqual(window.testClipboardFieldValueAt(1, expCompAckField),
                QStringLiteral("0"),
                QStringLiteral("Second same-page row should hydrate later ExpCompAck from its source page."));
    expectEqual(window.testClipboardFieldValueAt(0, traceTagField),
                QStringLiteral("0"),
                QStringLiteral("First same-page row should hydrate later TraceTag from its source page."));
    expectEqual(window.testClipboardFieldValueAt(1, traceTagField),
                QStringLiteral("1"),
                QStringLiteral("Second same-page row should hydrate later TraceTag from its source page."));
    expectEqual(window.testClipboardFieldValueAt(2, retryField),
                QStringLiteral("edited"),
                QStringLiteral("Modified same-page row should not have its edited AllowRetry overwritten."));
    expectEqual(window.testClipboardFieldValueAt(2, traceTagField),
                QStringLiteral("0"),
                QStringLiteral("Modified same-page row should fill its missing later TraceTag value."));
    expectEqual(window.testClipboardFieldValueAt(3, retryField),
                QString(),
                QStringLiteral("Other-page row should not hydrate from the anchor page."));
    expectEqual(window.testClipboardFieldValueAt(3, expCompAckField),
                QString(),
                QStringLiteral("Other-page row should not hydrate later ExpCompAck from the anchor page."));
    expectEqual(window.testClipboardFieldValueAt(3, traceTagField),
                QString(),
                QStringLiteral("Other-page row should not hydrate later TraceTag from the anchor page."));
    expectEqual(window.testClipboardFieldValueAt(4, retryField),
                QStringLiteral("1"),
                QStringLiteral("Unchanged other-page row should keep its existing AllowRetry value."));
    expect(!window.testClipboardEntryModified(0),
           QStringLiteral("Hydrated clean row should remain clean."));
    expect(!window.testClipboardEntryModified(1),
           QStringLiteral("Second hydrated clean row should remain clean."));
    expect(window.testClipboardEntryModified(2),
           QStringLiteral("Modified row should remain modified after hydration."));
    expect(!window.testClipboardEntryModified(3),
           QStringLiteral("Untouched other-page clean row should remain clean."));
}

void testClipboardReadOnlyEditableModifiedAndNavigation()
{
    TransactionTraceFixture firstFixture = makeIndexedReadNoSnpTrace(QStringLiteral("clipboard_nav_a.clogb"), 1);
    TransactionTraceFixture secondFixture = makeIndexedReadNoSnpTrace(QStringLiteral("clipboard_nav_b.clogb"), 1);

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(firstFixture.tracePath),
           QStringLiteral("First Clipboard navigation session should open."));
    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Global);
    window.testSelectLogicalRow(2);
    expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Global),
           QStringLiteral("First session Clipboard global insertion should succeed."));

    expect(window.testLoadTraceFile(secondFixture.tracePath),
           QStringLiteral("Second Clipboard navigation session should open."));
    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Global);
    window.testSelectLogicalRow(0);
    expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Global),
           QStringLiteral("Second session Clipboard global insertion should succeed."));

    expect(window.testClipboardReadOnly(),
           QStringLiteral("Clipboard should remain read-only after insertion."));
    expect(!window.testEditClipboardTimestampAt(0, 123456),
           QStringLiteral("Read-only Clipboard should reject cell edits."));

    window.testActivateClipboardRow(1);
    QApplication::processEvents();
    expectEqual(window.testActiveSessionIndex(),
                0,
                QStringLiteral("Read-only Clipboard activation should switch to the source session."));
    expectEqual(window.testSelectedLogicalRow(),
                2,
                QStringLiteral("Read-only Clipboard activation should select the source logical row."));

    expect(window.testSwitchToSession(1),
           QStringLiteral("Switching back to the second session should succeed."));
    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Global);
    window.testSetClipboardReadOnly(false);
    expect(!window.testClipboardReadOnly(),
           QStringLiteral("Clipboard should enter editable mode."));
    expect(window.testEditClipboardTimestampAt(0, 123456),
           QStringLiteral("Editable Clipboard should accept a cell edit."));
    expect(window.testClipboardHasModifiedRows(),
           QStringLiteral("Editing a Clipboard row should show modified state."));
    expect(window.testClipboardEntryModified(0),
           QStringLiteral("The edited Clipboard row should be marked modified."));

    window.testActivateClipboardRow(0);
    QApplication::processEvents();
    expectEqual(window.testActiveSessionIndex(),
                1,
                QStringLiteral("Editable-mode Clipboard activation should not jump sessions."));
    expectEqual(window.testSelectedLogicalRow(),
                0,
                QStringLiteral("Editable-mode Clipboard activation should not change source selection."));

    window.testSetClipboardReadOnly(true);
    expect(window.testClipboardReadOnly(),
           QStringLiteral("Clipboard should return to read-only mode."));
    window.testActivateClipboardRow(0);
    QApplication::processEvents();
    expectEqual(window.testActiveSessionIndex(),
                1,
                QStringLiteral("Read-only activation of a modified Clipboard row should not jump sessions."));
    expectEqual(window.testSelectedLogicalRow(),
                0,
                QStringLiteral("Read-only activation of a modified Clipboard row should not change source selection."));
}

void testClipboardCsvSaveIncludesFilteredRows()
{
    std::vector<CHIron::Gui::FlitRecord> rows = buildOptionalFieldStateRows();
    rows[1].summary = QStringLiteral("CSV, quoted \"summary\"");
    rows[2].annotation = QStringLiteral("line1\nline2");
    CHIron::Gui::FieldEntry specialField;
    specialField.scope = CHIron::Gui::InternFieldText(QStringLiteral("REQ"));
    specialField.name = CHIron::Gui::InternFieldText(QStringLiteral("Custom,Field"));
    specialField.value = QStringLiteral("decoded \"value\"");
    specialField.raw = QStringLiteral("0xA, raw");
    rows[0].fields.push_back(std::move(specialField));

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testApplyTraceRows(std::move(rows), QStringLiteral("clipboard_csv.clogb")),
           QStringLiteral("Clipboard CSV save session should open."));
    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Session);
    for (int row = 0; row < 4; ++row) {
        window.testSelectLogicalRow(row);
        expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Session),
               QStringLiteral("Clipboard CSV row insertion should succeed."));
    }

    window.testSetClipboardOpcodeFilter(QStringLiteral("Read"));
    QApplication::processEvents();
    expectEqual(window.testClipboardVisibleRowCount(),
                1,
                QStringLiteral("Clipboard CSV test should hide filtered-out rows before save."));

    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Temporary Clipboard CSV directory creation failed."));
    const QString csvPath = tempDir.filePath(QStringLiteral("clipboard.csv"));
    expect(window.testSaveClipboardCsv(csvPath),
           QStringLiteral("Clipboard CSV save should succeed."));

    QFile csv(csvPath);
    expect(csv.open(QIODevice::ReadOnly | QIODevice::Text),
           QStringLiteral("Saved Clipboard CSV should be readable."));
    const QString text = QString::fromUtf8(csv.readAll());
    expect(text.contains(QStringLiteral("Time,Channel,Direction,Opcode")),
           QStringLiteral("Clipboard CSV should include the table header."));
    expect(text.contains(QStringLiteral("ReadNotSharedDirty")),
           QStringLiteral("Clipboard CSV should include the visible row."));
    expect(text.contains(QStringLiteral("CompData")),
           QStringLiteral("Clipboard CSV should include rows hidden by filters."));
    expect(text.contains(QStringLiteral("\"CSV, quoted \"\"summary\"\"\"")),
           QStringLiteral("Clipboard CSV should escape quotes and commas."));
    expect(text.contains(QStringLiteral("\"line1\nline2\"")),
           QStringLiteral("Clipboard CSV should escape embedded newlines."));
    expect(text.contains(QStringLiteral("REQ.QoS")),
           QStringLiteral("Clipboard CSV should include applicable decoded field headers."));
    expect(text.contains(QStringLiteral("REQ.QoS.Raw")),
           QStringLiteral("Clipboard CSV should include applicable raw field headers."));
    expect(text.contains(QStringLiteral("\"REQ.Custom,Field\"")),
           QStringLiteral("Clipboard CSV should escape applicable field headers."));
    expect(text.contains(QStringLiteral("\"decoded \"\"value\"\"\"")),
           QStringLiteral("Clipboard CSV should include applicable decoded field values."));
    expect(text.contains(QStringLiteral("\"0xA, raw\"")),
           QStringLiteral("Clipboard CSV should include applicable raw field values."));
}

void testClipboardCLogBSaveAvailabilityAndRoundTrip()
{
    TransactionTraceFixture fixture = makeIndexedReadNoSnpTrace(QStringLiteral("clipboard_save.clogb"), 1);

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(fixture.tracePath),
           QStringLiteral("Clipboard CLog.B save source trace should load."));
    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Session);
    window.testSelectLogicalRow(0);
    expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Session),
           QStringLiteral("Clipboard CLog.B request insertion should succeed."));
    window.testSelectLogicalRow(1);
    expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Session),
           QStringLiteral("Clipboard CLog.B data insertion should succeed."));
    expect(window.testClipboardCLogBSaveAvailable(),
           QStringLiteral("Session Clipboard rows from one raw trace should be CLog.B-saveable."));

    const QString savedPath = fixture.tempDir.filePath(QStringLiteral("clipboard_saved.clogb"));
    expect(window.testSaveClipboardCLogB(savedPath),
           QStringLiteral("Clipboard CLog.B save should succeed."));
    CHIron::Gui::CLogBTraceMetadata metadata;
    CHIron::Gui::CLogBTraceLoadError error;
    expect(CHIron::Gui::CLogBTraceLoader::scanFile(savedPath, metadata, error),
           error.summary.isEmpty()
               ? QStringLiteral("Saved Clipboard CLog.B should scan.")
               : error.summary);
    expectEqual(static_cast<int>(metadata.totalRecords),
                2,
                QStringLiteral("Saved Clipboard CLog.B should contain the Clipboard rows."));

    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Global);
    window.testSelectLogicalRow(0);
    expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Global),
           QStringLiteral("Global Clipboard raw insertion should succeed."));

    std::vector<CHIron::Gui::FlitRecord> rowBackedRows = buildFilterTestRows();
    rowBackedRows[0].timestamp += 5000;
    expect(window.testApplyTraceRows(std::move(rowBackedRows), QStringLiteral("clipboard_row_backed.clogb")),
           QStringLiteral("Row-backed Clipboard source session should open."));
    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Global);
    window.testSelectLogicalRow(0);
    expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Global),
           QStringLiteral("Global Clipboard row-backed insertion should succeed."));
    expect(!window.testClipboardCLogBSaveAvailable(),
           QStringLiteral("Global Clipboard with row-backed rows should not be CLog.B-saveable."));
}

void testClipboardScopesAreSessionLocalAndGlobal()
{
    std::vector<CHIron::Gui::FlitRecord> firstRows = buildFilterTestRows();
    std::vector<CHIron::Gui::FlitRecord> secondRows = buildFilterTestRows();
    for (CHIron::Gui::FlitRecord& row : secondRows) {
        row.timestamp += 1000;
        row.opcode = QStringLiteral("SecondOnly");
    }

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testApplyTraceRows(std::move(firstRows), QStringLiteral("clipboard_a.clogb")),
           QStringLiteral("First Clipboard scope session should open."));
    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Session);
    window.testSelectLogicalRow(0);
    expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Session),
           QStringLiteral("First session Clipboard insertion should succeed."));
    expectEqual(window.testClipboardRowCount(),
                1,
                QStringLiteral("First session Clipboard should contain its inserted row."));

    expect(window.testApplyTraceRows(std::move(secondRows), QStringLiteral("clipboard_b.clogb")),
           QStringLiteral("Second Clipboard scope session should open."));
    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Session);
    expectEqual(window.testClipboardRowCount(),
                0,
                QStringLiteral("Second session Clipboard should start empty."));
    window.testSelectLogicalRow(0);
    expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Session),
           QStringLiteral("Second session Clipboard insertion should succeed."));
    expectEqual(window.testClipboardRowCount(),
                1,
                QStringLiteral("Second session Clipboard should contain its own row."));
    expectEqual(window.testClipboardOpcodeAt(0),
                QStringLiteral("SecondOnly"),
                QStringLiteral("Second session Clipboard should show the second session row."));

    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Global);
    window.testSelectLogicalRow(0);
    expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Global),
           QStringLiteral("Second session global Clipboard insertion should succeed."));
    expectEqual(window.testClipboardRowCount(),
                1,
                QStringLiteral("Global Clipboard should contain the second session row."));

    expect(window.testSwitchToSession(0),
           QStringLiteral("Switching back to first session should succeed."));
    QApplication::processEvents();
    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Session);
    expectEqual(window.testClipboardRowCount(),
                1,
                QStringLiteral("First session Clipboard should be restored when switching sessions."));
    expectEqual(window.testClipboardOpcodeAt(0),
                QStringLiteral("ReadNotSharedDirty"),
                QStringLiteral("First session Clipboard should still contain the first session row."));

    window.testSetClipboardScope(CHIron::Gui::ClipboardScope::Global);
    expectEqual(window.testClipboardRowCount(),
                1,
                QStringLiteral("Global Clipboard should persist across session switches."));
    window.testSelectLogicalRow(0);
    expect(window.testInsertSelectedFlitToClipboard(CHIron::Gui::ClipboardScope::Global),
           QStringLiteral("First session global Clipboard insertion should not collide with second session duplicate key."));
    expectEqual(window.testClipboardRowCount(),
                2,
                QStringLiteral("Global Clipboard should allow same logical row numbers from different sessions."));
}

void testMainWindowMarkersArePerSessionAndPersist()
{
    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testApplyTraceRows(buildTimelineTestRows(), QStringLiteral("marker_a.clogb")),
           QStringLiteral("First marker session should open."));
    expectEqual(window.testMarkerCount(),
                0,
                QStringLiteral("New session should start without markers."));
    expect(window.testAddMarkerAtLogicalRow(7),
           QStringLiteral("Adding a marker from the main trace row should succeed."));
    expectEqual(window.testMarkerCount(),
                1,
                QStringLiteral("Marker should be stored on the active session."));
    expect(window.testTraceMarkerOverlayVisible(),
           QStringLiteral("Main trace marker overlay should be visible when trace rows exist."));
    const QRect markerOverlayGeometry = window.testTraceMarkerOverlayGeometry();
    expect(markerOverlayGeometry.isValid(),
           QStringLiteral("Main trace marker overlay geometry should be valid."));
    expect(markerOverlayGeometry.width() > 18,
           QStringLiteral("Selected marker summary tag should expand the floating marker overlay."));
    const QRect cacheMinimapGeometry = window.testTraceCacheMinimapGeometry();
    expect(cacheMinimapGeometry.isValid(),
           QStringLiteral("Cache minimap overlay should reserve the marker/global minimap strip."));
    expect(markerOverlayGeometry.intersects(cacheMinimapGeometry),
           QStringLiteral("Expanded marker summary tag should float over the cache minimap overlay."));
    expect(markerOverlayGeometry.left() < cacheMinimapGeometry.right() - 18,
           QStringLiteral("Expanded marker summary tag should be allowed to float over the cache minimap."));
    const QRect traceScrollBarGeometry = window.testTraceTableScrollBarGeometry();
    const QRect markerMinimapGeometry = window.testTraceMarkerMinimapGeometry();
    expect(markerMinimapGeometry.isValid(),
           QStringLiteral("Marker/global minimap strip should have valid geometry."));
    expectEqual(markerMinimapGeometry.right(),
                traceScrollBarGeometry.left() - 1,
                QStringLiteral("Marker/global minimap strip should stick to the left edge of the table scrollbar."));
    const QRect collapsedMarkerTag = window.testTraceMarkerCollapsedTagGeometry(0);
    expect(collapsedMarkerTag.isValid(),
           QStringLiteral("Collapsed marker summary tag should have valid geometry."));
    expect(collapsedMarkerTag.left() < markerMinimapGeometry.left()
               && collapsedMarkerTag.right() >= markerMinimapGeometry.left(),
           QStringLiteral("Collapsed marker summary tag should point at and stick to the minimap strip."));
    const QRect expandedMarkerTag = window.testTraceMarkerExpandedTagGeometry(0);
    expect(expandedMarkerTag.isValid(),
           QStringLiteral("Expanded marker summary tag should have valid geometry."));
    expect(expandedMarkerTag.width() > collapsedMarkerTag.width(),
           QStringLiteral("Selected or hovered marker summary tag should expand to fit the marker label area."));
    expectEqual(expandedMarkerTag.right(),
                markerMinimapGeometry.right(),
                QStringLiteral("Expanded marker summary tag should keep pointing at the minimap strip."));
    const QRect traceViewportGeometry = window.testTraceTableViewportGeometry();
    const QRect leftMarkerOverlayGeometry = window.testTraceMarkerLeftOverlayGeometry();
    expect(leftMarkerOverlayGeometry.isValid(),
           QStringLiteral("Left-edge row-coupled marker overlay should have valid geometry."));
    expectEqual(leftMarkerOverlayGeometry.left(),
                traceViewportGeometry.left(),
                QStringLiteral("Left-edge marker overlay should be flush with the trace table viewport left edge."));
    const QRect firstLeftPolygon = window.testTraceMarkerLeftPolygonGeometry(0);
    expect(firstLeftPolygon.isValid(),
           QStringLiteral("Visible marker row should draw a left-edge marker polygon."));
    expectEqual(firstLeftPolygon.left(),
                traceViewportGeometry.left(),
                QStringLiteral("Left-edge marker polygon tip should touch the trace table viewport left edge."));
    const QRect firstMarkerRowGeometry = window.testTraceTableLogicalRowViewportGeometry(7);
    expect(firstMarkerRowGeometry.isValid(),
           QStringLiteral("Marked trace table row should have valid viewport geometry."));
    expectEqual(firstLeftPolygon.center().y(),
                firstMarkerRowGeometry.center().y(),
                QStringLiteral("Left-edge marker polygon should be vertically centered on its table row."));
    expect(!window.testTraceMarkerLeftNameGeometry(0).isValid(),
           QStringLiteral("Left-edge marker name should be hidden before double-click toggle."));
    expect(window.testDoubleClickTraceMarkerLeftPolygon(0),
           QStringLiteral("Double-clicking the left-edge marker polygon should toggle the marker name display."));
    const QRect firstLeftName = window.testTraceMarkerLeftNameGeometry(0);
    expect(firstLeftName.isValid(),
           QStringLiteral("Left-edge marker name should be visible after double-click toggle."));
    expect(firstLeftName.left() <= firstLeftPolygon.right(),
           QStringLiteral("Left-edge marker name should attach to the marker polygon as one monolithic tag."));
    expectEqual(firstLeftName.center().y(),
                firstLeftPolygon.center().y(),
                QStringLiteral("Left-edge marker name should stay vertically aligned with the marker polygon."));
    expect(window.testDoubleClickTraceMarkerLeftPolygon(0),
           QStringLiteral("Double-clicking the left-edge marker polygon again should hide the marker name."));
    expect(!window.testTraceMarkerLeftNameGeometry(0).isValid(),
           QStringLiteral("Left-edge marker name should be hidden after the second double-click toggle."));
    expect(window.testAddMarkerAtLogicalRow(12),
           QStringLiteral("Adding a second marker should succeed for marker tag click testing."));
    const QRect secondCollapsedMarkerTag = window.testTraceMarkerCollapsedTagGeometry(1);
    const QRect secondExpandedMarkerTag = window.testTraceMarkerExpandedTagGeometry(1);
    expect(secondCollapsedMarkerTag.isValid() && secondExpandedMarkerTag.isValid(),
           QStringLiteral("Inactive marker summary tag geometry should be available."));
    expect(secondCollapsedMarkerTag.width() < secondExpandedMarkerTag.width(),
           QStringLiteral("Inactive markers should remain as mini tags until selected or hovered."));
    expectEqual(window.testSelectedMarkerLogicalRow(),
                12,
                QStringLiteral("Adding the second marker should select it."));
    const QRect secondLeftPolygon = window.testTraceMarkerLeftPolygonGeometry(1);
    expect(secondLeftPolygon.isValid(),
           QStringLiteral("Second visible marker row should draw a left-edge marker polygon."));
    expect(secondLeftPolygon.center().y() > firstLeftPolygon.center().y(),
           QStringLiteral("Left-edge marker polygons should be coupled to their own table rows."));
    const QRect secondMarkerRowGeometry = window.testTraceTableLogicalRowViewportGeometry(12);
    expect(secondMarkerRowGeometry.isValid(),
           QStringLiteral("Second marked trace table row should have valid viewport geometry."));
    expectEqual(secondLeftPolygon.center().y(),
                secondMarkerRowGeometry.center().y(),
                QStringLiteral("Second left-edge marker polygon should be vertically centered on its table row."));
    window.testSetTableScrollValue(8);
    QApplication::processEvents();
    const QRect firstScrolledLeftPolygon = window.testTraceMarkerLeftPolygonGeometry(0);
    expect(!firstScrolledLeftPolygon.isValid(),
           QStringLiteral("Left-edge marker polygon should hide when its table row leaves the viewport."));
    const QRect secondScrolledLeftPolygon = window.testTraceMarkerLeftPolygonGeometry(1);
    expect(secondScrolledLeftPolygon.isValid(),
           QStringLiteral("Left-edge marker polygon should stay visible while its table row remains in the viewport."));
    window.testSetTableScrollValue(0);
    QApplication::processEvents();
    window.testSetOpcodeFilter(QStringLiteral("NoSuchMarkerOpcode"));
    QApplication::processEvents();
    expect(!window.testTraceMarkerLeftPolygonGeometry(0).isValid(),
           QStringLiteral("Filtered-out marker rows should not draw left-edge marker polygons."));
    window.testSetOpcodeFilter(QString());
    QApplication::processEvents();
    window.testNavigateMarker(false);
    expectEqual(window.testSelectedMarkerLogicalRow(),
                7,
                QStringLiteral("Alt-Up marker navigation should select the previous marker by row."));
    window.testNavigateMarker(false);
    expectEqual(window.testSelectedMarkerLogicalRow(),
                12,
                QStringLiteral("Alt-Up marker navigation should wrap to the last marker."));
    window.testNavigateMarker(true);
    expectEqual(window.testSelectedMarkerLogicalRow(),
                7,
                QStringLiteral("Alt-Down marker navigation should wrap to the first marker."));
    window.testNavigateMarker(true);
    expectEqual(window.testSelectedMarkerLogicalRow(),
                12,
                QStringLiteral("Alt-Down marker navigation should move to the second marker."));
    const int scrollBeforeLeftMarkerClick = window.testTableScrollValue();
    expect(window.testClickTraceMarkerLeftPolygon(0),
           QStringLiteral("Clicking a left-edge marker polygon should be handled by the row overlay."));
    expectEqual(window.testSelectedMarkerLogicalRow(),
                7,
                QStringLiteral("Clicking a left-edge marker polygon should select that marker."));
    expectEqual(window.testTableScrollValue(),
                scrollBeforeLeftMarkerClick,
                QStringLiteral("Clicking a left-edge marker polygon should not jump or scroll the trace table."));
    expect(window.testClickTraceMarkerLeftPolygon(0),
           QStringLiteral("Clicking a selected left-edge marker polygon should be handled by the row overlay."));
    expectEqual(window.testSelectedMarkerLogicalRow(),
                -1,
                QStringLiteral("Clicking a selected left-edge marker polygon should unselect that marker."));
    expect(window.testClickTraceMarkerTag(0),
           QStringLiteral("Clicking a marker summary tag should be handled by the marker overlay."));
    expectEqual(window.testSelectedMarkerLogicalRow(),
                7,
                QStringLiteral("Clicking a marker summary tag should select and jump to that marker row."));
    expect(window.testClickTraceMarkerTag(0),
           QStringLiteral("Clicking a selected marker summary tag should be handled by the marker overlay."));
    expectEqual(window.testSelectedMarkerLogicalRow(),
                -1,
                QStringLiteral("Clicking a selected marker summary tag should unselect that marker."));
    expect(window.testStartTraceMarkerMoveFromTag(0),
           QStringLiteral("Starting marker move from the right summary tag should enter move mode."));
    expect(window.testTraceMarkerMoveActive(),
           QStringLiteral("Marker move mode should be active after choosing Move from the right-side tag."));
    expect(window.testDropTraceMarkerMoveOnLogicalRow(20),
           QStringLiteral("Dropping a right-tag marker move on a visible row should be accepted."));
    expect(!window.testTraceMarkerMoveActive(),
           QStringLiteral("Successful marker move should leave move mode."));
    expectEqual(window.testSelectedMarkerLogicalRow(),
                20,
                QStringLiteral("Right-tag marker move should select the moved marker at its new row."));
    expectEqual(window.testMarkerTimestampAt(0),
                1200,
                QStringLiteral("Moved marker timestamp should update from the destination row."));
    expectEqual(window.testMarkerLabelAt(0),
                QStringLiteral("Marker 8"),
                QStringLiteral("Moved marker should keep its existing label."));
    expect(window.testEditMarkerDetails(0,
                                        QStringLiteral("Moved Anchor"),
                                        QStringLiteral("#3366cc"),
                                        QStringLiteral("moved memo")),
           QStringLiteral("Editing a marker immediately after moving it should not crash."));
    expectEqual(window.testMarkerLabelAt(0),
                QStringLiteral("Moved Anchor"),
                QStringLiteral("Moved marker label should update after edit."));
    expectEqual(window.testMarkerColorAt(0),
                QStringLiteral("#3366cc"),
                QStringLiteral("Moved marker color should update after edit."));
    expectEqual(window.testMarkerMemoAt(0),
                QStringLiteral("moved memo"),
                QStringLiteral("Moved marker memo should update after edit."));
    expect(window.testStartTraceMarkerMoveFromLeftPolygon(0),
           QStringLiteral("Starting marker move from the left row polygon should enter move mode."));
    expect(window.testDropTraceMarkerMoveOnLogicalRow(7),
           QStringLiteral("Dropping a left-polygon marker move on a visible row should be accepted."));
    expectEqual(window.testSelectedMarkerLogicalRow(),
                7,
                QStringLiteral("Left-polygon marker move should select the moved marker at its new row."));
    expectEqual(window.testMarkerTimestampAt(0),
                1070,
                QStringLiteral("Left-polygon move should refresh timestamp from the destination row."));
    expect(window.testStartTraceMarkerMoveFromTag(0),
           QStringLiteral("Starting marker move for occupied-row rejection should enter move mode."));
    expect(window.testDropTraceMarkerMoveOnLogicalRow(12),
           QStringLiteral("Dropping onto an occupied row should still emit the move request."));
    expect(window.testTraceMarkerMoveActive(),
           QStringLiteral("Rejected marker move should keep move mode active for another target."));
    expectEqual(window.testSelectedMarkerLogicalRow(),
                7,
                QStringLiteral("Rejected marker move should leave the marker on its original row."));
    window.testCancelTraceMarkerMove();
    expect(!window.testTraceMarkerMoveActive(),
           QStringLiteral("Explicit marker move cancellation should leave move mode."));
    expect(window.testStartTraceMarkerMoveFromTag(0),
           QStringLiteral("Starting marker move for same-row drop should enter move mode."));
    expect(window.testDropTraceMarkerMoveOnLogicalRow(7),
           QStringLiteral("Dropping a marker onto its existing row should be accepted as a no-op."));
    expect(!window.testTraceMarkerMoveActive(),
           QStringLiteral("Same-row marker move should leave move mode."));
    expectEqual(window.testSelectedMarkerLogicalRow(),
                7,
                QStringLiteral("Same-row marker move should keep the marker selected."));
    expect(window.testStartTraceMarkerMoveFromTag(0),
           QStringLiteral("Starting marker move for cancel testing should enter move mode."));
    window.testCancelTraceMarkerMove();
    expect(!window.testTraceMarkerMoveActive(),
           QStringLiteral("Cancelled marker move should leave move mode."));
    expectEqual(window.testSelectedMarkerLogicalRow(),
                7,
                QStringLiteral("Cancelled marker move should not move the marker."));
    expect(window.testRemoveMarkerAt(1),
           QStringLiteral("Removing the extra marker should restore the single-marker persistence setup."));
    expectEqual(window.testMarkerCount(),
                1,
                QStringLiteral("Only the original marker should be persisted after marker tag click testing."));
    expect(window.testEditMarkerDetails(0,
                                        QStringLiteral("Anchor A"),
                                        QStringLiteral("#00aa88"),
                                        QStringLiteral("memo text")),
           QStringLiteral("Editing marker label/color/memo should succeed."));

    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Marker test temporary directory should be valid."));
    const QString markerPath = tempDir.filePath(QStringLiteral("markers.markers.json"));
    expect(window.testSaveMarkersJson(markerPath),
           QStringLiteral("Marker sidecar JSON should save."));

    expect(window.testApplyTraceRows(buildSecondSessionSwitchRows(), QStringLiteral("marker_b.clogb")),
           QStringLiteral("Second marker session should open."));
    expectEqual(window.testMarkerCount(),
                0,
                QStringLiteral("Markers should be per-session and not leak to a new trace session."));
    expect(window.testTraceMarkerOverlayVisible(),
           QStringLiteral("Marker overlay should stay visible as a global viewport minimap without markers."));

    expect(window.testLoadMarkersJson(markerPath),
           QStringLiteral("Marker sidecar JSON should load into the active session."));
    expectEqual(window.testMarkerCount(),
                1,
                QStringLiteral("Loaded marker count should match the saved sidecar."));
    expectEqual(window.testSelectedMarkerLogicalRow(),
                7,
                QStringLiteral("Loading markers should select the first valid marker."));
    expectEqual(window.testMarkerLabelAt(0),
                QStringLiteral("Anchor A"),
                QStringLiteral("Marker label should round-trip through JSON."));
    expectEqual(window.testMarkerColorAt(0),
                QStringLiteral("#00aa88"),
                QStringLiteral("Marker color should round-trip through JSON."));
    expectEqual(window.testMarkerMemoAt(0),
                QStringLiteral("memo text"),
                QStringLiteral("Marker memo should round-trip through JSON."));
    expect(window.testTraceMarkerOverlayVisible(),
           QStringLiteral("Main trace marker overlay should show after loading markers."));

    expect(window.testSwitchToSession(0),
           QStringLiteral("Switching back to first marker session should succeed."));
    QApplication::processEvents();
    expectEqual(window.testMarkerCount(),
                1,
                QStringLiteral("First session should keep its original marker."));
    expectEqual(window.testMarkerLabelAt(0),
                QStringLiteral("Anchor A"),
                QStringLiteral("First session marker edit should be retained."));
    expect(window.testRemoveMarkerAt(0),
           QStringLiteral("Removing a marker should succeed."));
    expectEqual(window.testMarkerCount(),
                0,
                QStringLiteral("Removed marker should leave the session marker list empty."));
    expect(window.testTraceMarkerOverlayVisible(),
           QStringLiteral("Marker overlay should remain visible after removing the last marker."));
}

void testMainWindowMarkerUndoRedoAndUnifiedOrdering()
{
    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testApplyTraceRows(buildTimelineTestRows(), QStringLiteral("marker_undo.clogb")),
           QStringLiteral("Marker undo test session should open."));
    expect(window.testAddMarkerAtLogicalRow(7),
           QStringLiteral("Adding a marker should create an undoable marker command."));
    expect(window.testCanUndoUnified(),
           QStringLiteral("Unified undo should be available after adding a marker."));
    expect(window.testUnifiedUndoText().startsWith(QStringLiteral("Add marker")),
           QStringLiteral("Unified undo text should describe the marker add."));
    window.testUndoUnified();
    expectEqual(window.testMarkerCount(),
                0,
                QStringLiteral("Undoing marker add should remove the marker."));
    expect(window.testCanRedoUnified(),
           QStringLiteral("Unified redo should be available after undoing marker add."));
    window.testRedoUnified();
    expectEqual(window.testMarkerCount(),
                1,
                QStringLiteral("Redoing marker add should restore the marker."));
    expectEqual(window.testSelectedMarkerLogicalRow(),
                7,
                QStringLiteral("Redoing marker add should restore marker selection."));

    expect(window.testMoveMarkerAt(0, 20),
           QStringLiteral("Moving a marker should create an undoable marker command."));
    expectEqual(window.testSelectedMarkerLogicalRow(),
                20,
                QStringLiteral("Moved marker should land on the requested row."));
    expectEqual(window.testMarkerTimestampAt(0),
                1200,
                QStringLiteral("Moved marker timestamp should come from the target row."));
    window.testUndoUnified();
    expectEqual(window.testSelectedMarkerLogicalRow(),
                7,
                QStringLiteral("Undoing marker move should restore the original row."));
    expectEqual(window.testMarkerTimestampAt(0),
                1070,
                QStringLiteral("Undoing marker move should restore the original timestamp."));
    window.testRedoUnified();
    expectEqual(window.testSelectedMarkerLogicalRow(),
                20,
                QStringLiteral("Redoing marker move should reapply the target row."));
    expectEqual(window.testMarkerTimestampAt(0),
                1200,
                QStringLiteral("Redoing marker move should restore the target timestamp."));

    expect(window.testEditMarkerDetails(0,
                                        QStringLiteral("Undo Anchor"),
                                        QStringLiteral("#8855aa"),
                                        QStringLiteral("undo memo")),
           QStringLiteral("Editing marker details should create one undoable command."));
    window.testUndoUnified();
    expectEqual(window.testMarkerLabelAt(0),
                QStringLiteral("Marker 8"),
                QStringLiteral("Undoing marker edit should restore the old label."));
    window.testRedoUnified();
    expectEqual(window.testMarkerLabelAt(0),
                QStringLiteral("Undo Anchor"),
                QStringLiteral("Redoing marker edit should restore the new label."));
    expectEqual(window.testMarkerMemoAt(0),
                QStringLiteral("undo memo"),
                QStringLiteral("Redoing marker edit should restore the new memo."));

    expect(window.testAddMarkerAtLogicalRow(12),
           QStringLiteral("Adding a second marker should succeed."));
    const QString undoTextBeforeRejectedMove = window.testUnifiedUndoText();
    expect(!window.testMoveMarkerAt(0, 12),
           QStringLiteral("Moving onto an occupied marker row should be rejected."));
    expectEqual(window.testUnifiedUndoText(),
                undoTextBeforeRejectedMove,
                QStringLiteral("Rejected occupied-row moves should not add an undo step."));
    expect(window.testRemoveMarkerAt(1),
           QStringLiteral("Deleting a marker should create an undoable marker command."));
    expectEqual(window.testMarkerCount(),
                1,
                QStringLiteral("Deleting the second marker should reduce marker count."));
    window.testUndoUnified();
    expectEqual(window.testMarkerCount(),
                2,
                QStringLiteral("Undoing marker delete should restore the deleted marker."));
    window.testRedoUnified();
    expectEqual(window.testMarkerCount(),
                1,
                QStringLiteral("Redoing marker delete should delete the marker again."));

    expect(window.testSetTraceEditable(true),
           QStringLiteral("Test should enable trace editing without modal UI."));
    const int originalTimestamp = static_cast<int>(window.testTraceTimestampAtLogicalRow(0));
    expect(window.testEditTraceTimestampAtLogicalRow(0, 7777),
           QStringLiteral("Trace timestamp edit should create a flit undo route."));
    expect(window.testMoveMarkerAt(0, 7),
           QStringLiteral("Marker move after a flit edit should become the most recent undo route."));
    expect(window.testUnifiedUndoText().startsWith(QStringLiteral("Move marker")),
           QStringLiteral("Unified undo should prefer the most recent marker move over the older flit edit."));
    window.testUndoUnified();
    expectEqual(window.testMarkerLogicalRowAt(0),
                20,
                QStringLiteral("First unified undo should undo the marker move."));
    expectEqual(static_cast<int>(window.testTraceTimestampAtLogicalRow(0)),
                7777,
                QStringLiteral("First unified undo should leave the flit edit intact."));
    window.testUndoUnified();
    expectEqual(static_cast<int>(window.testTraceTimestampAtLogicalRow(0)),
                originalTimestamp,
                QStringLiteral("Second unified undo should undo the older flit edit."));
    window.testRedoUnified();
    expectEqual(static_cast<int>(window.testTraceTimestampAtLogicalRow(0)),
                7777,
                QStringLiteral("First unified redo should redo the flit edit."));
    window.testRedoUnified();
    expectEqual(window.testMarkerLogicalRowAt(0),
                7,
                QStringLiteral("Second unified redo should redo the marker move."));

    window.testUndoUnified();
    expect(window.testCanRedoUnified(),
           QStringLiteral("Redo should be available after undoing the marker move."));
    expect(window.testEditMarkerLabel(0, QStringLiteral("Redo Breaker")),
           QStringLiteral("A new marker edit after undo should clear unified redo."));
    expect(!window.testCanRedoUnified(),
           QStringLiteral("New marker command after undo should clear unified redo history."));
}

void testTraceIssueSeverityAndErrorsWidget()
{
    using CHIron::Gui::CountTraceIssues;
    using CHIron::Gui::ErrorsWidget;
    using CHIron::Gui::TraceIssueDisposition;
    using CHIron::Gui::TraceIssue;
    using CHIron::Gui::TraceIssueDenialIsReportable;
    using CHIron::Gui::TraceIssueSeverity;
    using CHIron::Gui::TraceIssueSeverityForDenial;
    using CHIron::Gui::TraceIssueSource;

    expect(!TraceIssueDenialIsReportable(QString()),
           QStringLiteral("Empty denials should not be reported as Errors issues."));
    expect(!TraceIssueDenialIsReportable(QStringLiteral("XACT_ACCEPTED")),
           QStringLiteral("Accepted xaction rows should not be reported as Errors issues."));
    expect(!TraceIssueDenialIsReportable(QStringLiteral("not processed")),
           QStringLiteral("Not-processed xaction rows should not be reported as Errors issues."));
    expect(TraceIssueDenialIsReportable(QStringLiteral("DENIED_FIELD_MAPPING")),
           QStringLiteral("Denied xaction/cache rows should be reportable."));
    expect(TraceIssueSeverityForDenial(QStringLiteral("DENIED_FIELD_MAPPING")) == TraceIssueSeverity::Error,
           QStringLiteral("All current denied xaction/cache denials should be classified as errors."));

    TraceIssue cacheIssue;
    cacheIssue.id = QStringLiteral("issue-1");
    cacheIssue.logicalRow = 41;
    cacheIssue.timestamp = 1041;
    cacheIssue.severity = TraceIssueSeverity::Error;
    cacheIssue.source = TraceIssueSource::CacheStateReplay;
    cacheIssue.denialName = QStringLiteral("DENIED_STATE_TRANSITION");
    cacheIssue.denialCode = QStringLiteral("0x123");
    cacheIssue.channel = QStringLiteral("DAT");
    cacheIssue.opcode = QStringLiteral("DataSepResp");
    cacheIssue.node = QStringLiteral("2 RN");
    cacheIssue.address = QStringLiteral("0x1000");
    cacheIssue.summary = QStringLiteral("Cache replay denied DataSepResp at row 42");
    cacheIssue.details = QStringLiteral("Denial code: 0x123\nJoint type: RNCacheStateMap\nLine address: 0x1000");

    TraceIssue xactionIssue = cacheIssue;
    xactionIssue.id = QStringLiteral("issue-2");
    xactionIssue.logicalRow = 43;
    xactionIssue.source = TraceIssueSource::XactionIndex;
    xactionIssue.denialName = QStringLiteral("XACT_DENIED_RSP_TXNID_NOT_EXIST");
    xactionIssue.denialCode = QStringLiteral("0x456");
    xactionIssue.summary = QStringLiteral("Xaction denied RSP at row 44");

    const std::vector<TraceIssue> issues{cacheIssue};
    const auto counts = CountTraceIssues(issues);
    expectEqual(counts.errors,
                1,
                QStringLiteral("Denied diagnostics should contribute to the error count."));
    expectEqual(counts.warnings,
                0,
                QStringLiteral("Warning count should remain reserved for future diagnostics."));

    ErrorsWidget widget;
    widget.resize(760, 320);
    widget.show();
    QApplication::processEvents();
    expectEqual(widget.testIssueDispositionText(TraceIssueSource::XactionIndex),
                QStringLiteral("Error"),
                QStringLiteral("Errors widget should default Xaction denials to Error."));
    expectEqual(widget.testIssueDispositionText(TraceIssueSource::CacheStateReplay),
                QStringLiteral("Error"),
                QStringLiteral("Errors widget should default Cache State denials to Error."));
    expect(widget.testSeverityVisible(TraceIssueSeverity::Error),
           QStringLiteral("Errors widget should show errors by default."));
    expect(widget.testSeverityVisible(TraceIssueSeverity::Warning),
           QStringLiteral("Errors widget should show warnings by default."));
    expect(widget.testSeverityButtonHasIcon(TraceIssueSeverity::Error),
           QStringLiteral("Errors widget Error count toggle should show its critical icon."));
    expect(widget.testSeverityButtonHasIcon(TraceIssueSeverity::Warning),
           QStringLiteral("Errors widget Warning count toggle should show its warning icon."));
    expectEqual(widget.testDispositionButtonText(TraceIssueSource::XactionIndex),
                QStringLiteral("Xaction: Error"),
                QStringLiteral("Xaction disposition should be one current-state button."));
    expectEqual(widget.testDispositionButtonText(TraceIssueSource::CacheStateReplay),
                QStringLiteral("Cache: Error"),
                QStringLiteral("Cache disposition should be one current-state button."));
    expect(widget.testDispositionButtonHasIcon(TraceIssueSource::XactionIndex),
           QStringLiteral("Xaction Error disposition button should show a critical icon."));
    expect(widget.testDispositionButtonHasIcon(TraceIssueSource::CacheStateReplay),
           QStringLiteral("Cache Error disposition button should show a critical icon."));
    expect(!widget.testDispositionButtonCheckable(TraceIssueSource::XactionIndex),
           QStringLiteral("Xaction disposition button should not use a checked selected fill."));
    expect(!widget.testDispositionButtonChecked(TraceIssueSource::XactionIndex),
           QStringLiteral("Xaction disposition button should remain visually unselected."));
    expect(!widget.testDispositionButtonCheckable(TraceIssueSource::CacheStateReplay),
           QStringLiteral("Cache disposition button should not use a checked selected fill."));
    expect(!widget.testDispositionButtonChecked(TraceIssueSource::CacheStateReplay),
           QStringLiteral("Cache disposition button should remain visually unselected."));
    expect(widget.testClickDispositionButton(TraceIssueSource::CacheStateReplay),
           QStringLiteral("Clicking the Cache disposition button should cycle its disposition."));
    expectEqual(widget.testIssueDispositionText(TraceIssueSource::CacheStateReplay),
                QStringLiteral("Warning"),
                QStringLiteral("Cache disposition button should cycle Error to Warning."));
    expectEqual(widget.testDispositionButtonText(TraceIssueSource::CacheStateReplay),
                QStringLiteral("Cache: Warning"),
                QStringLiteral("Cache disposition button should display Warning after one click."));
    expect(widget.testDispositionButtonHasIcon(TraceIssueSource::CacheStateReplay),
           QStringLiteral("Cache Warning disposition button should show a warning icon."));
    expect(!widget.testDispositionButtonChecked(TraceIssueSource::CacheStateReplay),
           QStringLiteral("Cache disposition button should stay visually unselected after cycling to Warning."));
    expect(widget.testClickDispositionButton(TraceIssueSource::CacheStateReplay),
           QStringLiteral("Clicking the Cache disposition button should cycle from Warning."));
    expectEqual(widget.testIssueDispositionText(TraceIssueSource::CacheStateReplay),
                QStringLiteral("Ignored"),
                QStringLiteral("Cache disposition button should cycle Warning to Ignored."));
    expectEqual(widget.testDispositionButtonText(TraceIssueSource::CacheStateReplay),
                QStringLiteral("Cache: Ignored"),
                QStringLiteral("Cache disposition button should display Ignored after two clicks."));
    expect(!widget.testDispositionButtonHasIcon(TraceIssueSource::CacheStateReplay),
           QStringLiteral("Ignored disposition button should be text-only."));
    expect(!widget.testDispositionButtonChecked(TraceIssueSource::CacheStateReplay),
           QStringLiteral("Cache disposition button should stay visually unselected after cycling to Ignored."));
    expect(widget.testClickDispositionButton(TraceIssueSource::CacheStateReplay),
           QStringLiteral("Clicking the Cache disposition button should cycle from Ignored."));
    expectEqual(widget.testIssueDispositionText(TraceIssueSource::CacheStateReplay),
                QStringLiteral("Error"),
                QStringLiteral("Cache disposition button should cycle Ignored back to Error."));
    widget.setBuildStatus(false, QStringLiteral("Build Xaction index to collect protocol errors."));
    expect(widget.testStatusText().contains(QStringLiteral("Build Xaction index")),
           QStringLiteral("Errors widget should show non-active build guidance text."));
    expect(!widget.testProgressVisible(),
           QStringLiteral("Errors widget progress bar should stay hidden for inactive guidance text."));
    widget.setBuildStatus(true, QStringLiteral("Loading persisted errors"), 25, 100);
    QApplication::processEvents();
    expect(widget.testProgressVisible(),
           QStringLiteral("Errors widget progress bar should show during active loading."));
    expect(!widget.testProgressBusy(),
           QStringLiteral("Errors widget progress bar should be determinate when a total is known."));
    expectEqual(widget.testProgressMaximum(),
                1000,
                QStringLiteral("Errors widget progress bar should use a compact normalized determinate range."));
    expectEqual(widget.testProgressValue(),
                250,
                QStringLiteral("Errors widget progress bar should scale completed issue loading progress."));
    expect(widget.testStatusText().contains(QStringLiteral("25 / 100 issues")),
           QStringLiteral("Errors widget status should report persisted issue loading progress."));
    widget.setBuildStatus(true, QStringLiteral("Loading persisted errors"));
    QApplication::processEvents();
    expect(widget.testProgressVisible(),
           QStringLiteral("Errors widget progress bar should still show when total progress is unknown."));
    expect(widget.testProgressBusy(),
           QStringLiteral("Errors widget progress bar should become indeterminate without a total."));
    widget.setIssues(issues);
    QApplication::processEvents();
    expect(!widget.testProgressVisible(),
           QStringLiteral("Errors widget progress bar should hide after issues are loaded."));

    expectEqual(widget.issueCount(),
                1,
                QStringLiteral("Errors widget should list one issue."));
    expectEqual(widget.errorCount(),
                1,
                QStringLiteral("Errors widget toolbar should count errors."));
    expectEqual(widget.warningCount(),
                0,
                QStringLiteral("Errors widget toolbar should keep warnings at zero for current denials."));
    expect(widget.testTopLevelHasSeverityIcon(0),
           QStringLiteral("Errors widget rows should carry a severity icon."));
    expect(widget.testIssueTreeRootDecorated(),
           QStringLiteral("Errors widget should use the native tree expander control."));
    expectGreater(widget.testIssueTreeIndentation(),
                  0.0,
                  QStringLiteral("Errors widget should reserve space for the expand/fold indicator."));
    expectEqual(widget.testCodeText(0),
                cacheIssue.denialCode,
                QStringLiteral("Errors widget Code column should prefer denial code."));
    expectEqual(widget.testDescriptionText(0),
                cacheIssue.summary,
                QStringLiteral("Errors widget Description column should display the issue summary."));
    expectEqual(widget.testSourceText(0),
                QStringLiteral("Cache State"),
                QStringLiteral("Errors widget Source column should display the issue source."));
    expectEqual(widget.testRowText(0),
                QStringLiteral("42"),
                QStringLiteral("Errors widget Row column should display one-based trace rows."));
    expect(widget.testDetailText(0).contains(QStringLiteral("Joint type")),
           QStringLiteral("Expanded Errors widget details should expose denial context."));
    expect(!widget.testIssueExpanded(0),
           QStringLiteral("Errors widget issue rows should start collapsed."));
    expectEqual(widget.testIssueContextToggleText(0),
                QStringLiteral("Expand"),
                QStringLiteral("Collapsed Errors issue context action should offer Expand."));
    expect(widget.testSetIssueExpanded(0, true),
           QStringLiteral("Errors widget test API should expand an issue row."));
    expect(widget.testIssueExpanded(0),
           QStringLiteral("Errors widget issue row should report expanded after expansion."));
    expect(widget.testExpandedDetailText(0).contains(QStringLiteral("Joint type")),
           QStringLiteral("Expanded Errors issue detail child should show persisted denial details."));
    expectEqual(widget.testIssueContextToggleText(0),
                QStringLiteral("Fold"),
                QStringLiteral("Expanded Errors issue context action should offer Fold."));
    expect(widget.testSetIssueExpanded(0, false),
           QStringLiteral("Errors widget test API should fold an issue row."));

    int activatedRow = -1;
    QObject::connect(&widget, &ErrorsWidget::logicalRowActivated, &widget, [&](const int logicalRow) {
        activatedRow = logicalRow;
    });
    expect(widget.testActivateIssue(0),
           QStringLiteral("Errors widget test activation should target the issue row."));
    expectEqual(activatedRow,
                41,
                QStringLiteral("Double-clicking an Errors issue should activate its logical trace row."));
    expect(widget.testIssueExpanded(0),
           QStringLiteral("Double-clicking an Errors issue should also expand the row."));
    expect(widget.testActivateIssue(0),
           QStringLiteral("Second double-clicking an Errors issue should fold it again."));
    expect(!widget.testIssueExpanded(0),
           QStringLiteral("Second double-clicking an Errors issue should report folded state."));
    expect(widget.testClickDispositionButton(TraceIssueSource::CacheStateReplay),
           QStringLiteral("Errors widget Cache disposition button should cycle to Warning."));
    expectEqual(widget.testIssueDispositionText(TraceIssueSource::CacheStateReplay),
                QStringLiteral("Warning"),
                QStringLiteral("Errors widget should expose changed Cache State disposition."));
    expect(widget.testClickDispositionButton(TraceIssueSource::CacheStateReplay),
           QStringLiteral("Errors widget Cache disposition button should cycle to Ignored."));
    expectEqual(widget.testIssueDispositionText(TraceIssueSource::CacheStateReplay),
                QStringLiteral("Ignored"),
                QStringLiteral("Errors widget should expose ignored Cache State disposition."));
    expect(widget.testSetSeverityVisible(TraceIssueSeverity::Error, false),
           QStringLiteral("Errors widget test API should hide errors."));
    expect(!widget.testSeverityVisible(TraceIssueSeverity::Error),
           QStringLiteral("Errors widget should expose hidden error toggle state."));
    expect(widget.testSeverityButtonHasIcon(TraceIssueSeverity::Error),
           QStringLiteral("Errors widget Error count toggle should keep its icon while hidden."));
    expect(widget.testSeverityButtonText(TraceIssueSeverity::Error).contains(QStringLiteral("1 error")),
           QStringLiteral("Errors widget Error count toggle should keep its count while hidden."));
    expect(widget.testSetSeverityVisible(TraceIssueSeverity::Warning, true),
           QStringLiteral("Errors widget test API should keep warnings visible."));

    std::vector<TraceIssue> manyIssues;
    widget.testSetIssuePageSize(64);
    manyIssues.reserve(200);
    for (int index = 0; index < 200; ++index) {
        TraceIssue issue = xactionIssue;
        issue.id = QStringLiteral("paged-issue-%1").arg(index);
        issue.logicalRow = static_cast<std::uint64_t>(index);
        issue.summary = QStringLiteral("Paged issue at row %1").arg(index + 1);
        manyIssues.push_back(std::move(issue));
    }
    widget.setIssues(manyIssues);
    QApplication::processEvents();
    expectEqual(widget.issueCount(),
                200,
                QStringLiteral("Errors widget should keep the full issue count for paged lists."));
    expect(widget.testDisplayedIssueRowCount() < widget.issueCount(),
           QStringLiteral("Errors widget should initially expose only the first issue page to the view."));
    const int displayedBeforeFetch = widget.testDisplayedIssueRowCount();
    expect(widget.testFetchMoreIssueRows(),
           QStringLiteral("Errors widget should fetch additional issue pages on demand."));
    expect(widget.testDisplayedIssueRowCount() > displayedBeforeFetch,
           QStringLiteral("Errors widget fetchMore should increase the displayed issue page."));
    while (widget.testDisplayedIssueRowCount() < widget.issueCount()) {
        expect(widget.testFetchMoreIssueRows(),
               QStringLiteral("Errors widget should keep fetching additional issue pages until complete."));
    }
    expectEqual(widget.testDisplayedIssueRowCount(),
                widget.issueCount(),
                QStringLiteral("Errors widget should expose the remaining small page after fetchMore."));
    widget.close();
    QApplication::processEvents();
}

void testErrorsDockSourceDispositionAndSeverityFilters()
{
    resetErrorsDispositionSettings();

    CHIron::Gui::TraceIssue xactionIssue;
    xactionIssue.id = QStringLiteral("xaction-issue");
    xactionIssue.logicalRow = 4;
    xactionIssue.severity = CHIron::Gui::TraceIssueSeverity::Error;
    xactionIssue.source = CHIron::Gui::TraceIssueSource::XactionIndex;
    xactionIssue.denialName = QStringLiteral("XACT_DENIED_RSP_TXNID_NOT_EXIST");
    xactionIssue.denialCode = QStringLiteral("0x101");
    xactionIssue.summary = QStringLiteral("Xaction denial at row 5");
    xactionIssue.details = QStringLiteral("Xaction index denial");

    CHIron::Gui::TraceIssue cacheIssue;
    cacheIssue.id = QStringLiteral("cache-issue");
    cacheIssue.logicalRow = 8;
    cacheIssue.severity = CHIron::Gui::TraceIssueSeverity::Error;
    cacheIssue.source = CHIron::Gui::TraceIssueSource::CacheStateReplay;
    cacheIssue.denialName = QStringLiteral("XACT_DENIED_STATE_INITIAL");
    cacheIssue.denialCode = QStringLiteral("0x202");
    cacheIssue.summary = QStringLiteral("Cache denial at row 9");
    cacheIssue.details = QStringLiteral("Cache-state replay denial");

    const std::vector<CHIron::Gui::TraceIssue> rawIssues{xactionIssue, cacheIssue};
    std::vector<CHIron::Gui::TraceIssue> visibleIssues =
        CHIron::Gui::ApplyTraceIssueDispositionPolicy(rawIssues,
                                                      CHIron::Gui::TraceIssueDisposition::Error,
                                                      CHIron::Gui::TraceIssueDisposition::Warning);
    auto counts = CHIron::Gui::CountTraceIssues(visibleIssues);
    expectEqual(counts.errors,
                1,
                QStringLiteral("Xaction Error + Cache Warning should count one error."));
    expectEqual(counts.warnings,
                1,
                QStringLiteral("Xaction Error + Cache Warning should count one warning."));

    visibleIssues = CHIron::Gui::ApplyTraceIssueDispositionPolicy(rawIssues,
                                                                  CHIron::Gui::TraceIssueDisposition::Ignored,
                                                                  CHIron::Gui::TraceIssueDisposition::Warning);
    counts = CHIron::Gui::CountTraceIssues(visibleIssues);
    expectEqual(static_cast<int>(visibleIssues.size()),
                1,
                QStringLiteral("Ignoring Xaction should hide only Xaction issues."));
    expectEqual(counts.warnings,
                1,
                QStringLiteral("Cache Warning should remain visible when Xaction is ignored."));
    expectEqual(visibleIssues.front().source == CHIron::Gui::TraceIssueSource::CacheStateReplay ? 1 : 0,
                1,
                QStringLiteral("Remaining issue should be Cache State."));

    visibleIssues = CHIron::Gui::ApplyTraceIssueDispositionPolicy(rawIssues,
                                                                  CHIron::Gui::TraceIssueDisposition::Error,
                                                                  CHIron::Gui::TraceIssueDisposition::Ignored);
    counts = CHIron::Gui::CountTraceIssues(visibleIssues);
    expectEqual(static_cast<int>(visibleIssues.size()),
                1,
                QStringLiteral("Ignoring Cache State should hide only cache issues."));
    expectEqual(counts.errors,
                1,
                QStringLiteral("Xaction Error should remain visible when Cache State is ignored."));
    expectEqual(visibleIssues.front().source == CHIron::Gui::TraceIssueSource::XactionIndex ? 1 : 0,
                1,
                QStringLiteral("Remaining issue should be Xaction."));

    CHIron::Gui::ErrorsWidget widget;
    widget.resize(900, 320);
    widget.show();
    QApplication::processEvents();
    widget.setIssues(rawIssues);
    expectEqual(widget.errorCount(),
                2,
                QStringLiteral("Errors widget should start with both mixed issues as errors."));
    expect(widget.testSeverityButtonHasIcon(CHIron::Gui::TraceIssueSeverity::Error),
           QStringLiteral("Mixed-source Errors widget Error toggle should expose its icon."));
    expect(widget.testSeverityButtonHasIcon(CHIron::Gui::TraceIssueSeverity::Warning),
           QStringLiteral("Mixed-source Errors widget Warning toggle should expose its icon."));
    expect(widget.testSetSeverityVisible(CHIron::Gui::TraceIssueSeverity::Error, false),
           QStringLiteral("Errors widget should expose the Error visibility toggle."));
    expect(!widget.testSeverityVisible(CHIron::Gui::TraceIssueSeverity::Error),
           QStringLiteral("Errors widget should remember hidden Error toggle state."));
    expect(widget.testSeverityButtonText(CHIron::Gui::TraceIssueSeverity::Error).contains(QStringLiteral("2 errors")),
           QStringLiteral("Errors widget count text should not clear when errors are toggled off."));
    widget.close();
}

void testMarkerWidgetSearchFiltersNameAndMemo()
{
    using CHIron::Gui::MarkerWidget;
    using CHIron::Gui::TraceMarker;
    using CHIron::Gui::TraceMarkerDisplaySummary;

    MarkerWidget widget;
    widget.resize(520, 420);
    widget.show();
    QApplication::processEvents();

    std::vector<TraceMarker> markers;
    TraceMarker alpha;
    alpha.id = QStringLiteral("alpha");
    alpha.logicalRow = 3;
    alpha.timestamp = 1030;
    alpha.label = QStringLiteral("Alpha anchor");
    alpha.color = QColor(QStringLiteral("#3366cc"));
    alpha.memo = QStringLiteral("first marker memo");
    markers.push_back(alpha);

    TraceMarker beta;
    beta.id = QStringLiteral("beta");
    beta.logicalRow = 7;
    beta.timestamp = 1070;
    beta.label = QStringLiteral("Beta checkpoint");
    beta.color = QColor(QStringLiteral("#00aa88"));
    beta.memo = QStringLiteral("contains bus retry details");
    markers.push_back(beta);

    TraceMarker gamma;
    gamma.id = QStringLiteral("gamma");
    gamma.logicalRow = 11;
    gamma.timestamp = 1110;
    gamma.label = QStringLiteral("Gamma watch");
    gamma.color = QColor(QStringLiteral("#8855aa"));
    gamma.memo = QStringLiteral("ordinary note");
    markers.push_back(gamma);

    std::vector<TraceMarkerDisplaySummary> summaries;
    summaries.push_back(TraceMarkerDisplaySummary{QStringLiteral("alpha"),
                                                  QStringLiteral("REQ"),
                                                  QStringLiteral("ReadShared"),
                                                  QStringLiteral("0x1000")});
    summaries.push_back(TraceMarkerDisplaySummary{QStringLiteral("beta"),
                                                  QStringLiteral("RSP"),
                                                  QStringLiteral("CompAck"),
                                                  QString()});
    summaries.push_back(TraceMarkerDisplaySummary{QStringLiteral("gamma"),
                                                  QStringLiteral("SNP"),
                                                  QStringLiteral("SnpCleanWithLongIntegratedSummary"),
                                                  QStringLiteral("0x200000000000000000000000")});

    widget.setMarkers(markers, QStringLiteral("beta"), CHIron::Gui::MarkerStickyState{}, summaries);
    QApplication::processEvents();

    auto* table = widget.findChild<QTableWidget*>(QStringLiteral("markerTable"));
    auto* searchEdit = widget.findChild<QLineEdit*>(QStringLiteral("markerSearchEdit"));
    expect(table != nullptr, QStringLiteral("Marker widget should expose its marker table."));
    expect(searchEdit != nullptr, QStringLiteral("Marker widget should expose its search field."));
    expectEqual(table->columnCount(),
                7,
                QStringLiteral("Marker list should expose summary columns."));
    expectEqual(table->horizontalHeaderItem(1)->text(),
                QStringLiteral("Channel"),
                QStringLiteral("Marker list should show a Channel column."));
    expectEqual(table->horizontalHeaderItem(2)->text(),
                QStringLiteral("Opcode"),
                QStringLiteral("Marker list should show an Opcode column."));
    expectEqual(table->horizontalHeaderItem(3)->text(),
                QStringLiteral("Address"),
                QStringLiteral("Marker list should show an Address column."));
    expect(table->verticalHeader()->defaultSectionSize() <= 22,
           QStringLiteral("Marker list rows should use a compact default height."));
    expect(!table->wordWrap(),
           QStringLiteral("Marker list should not wrap compact summary rows."));
    expectEqual(table->rowCount(),
                3,
                QStringLiteral("Empty marker search should show all markers."));
    expectEqual(table->item(0, 1)->text(),
                QStringLiteral("REQ"),
                QStringLiteral("Marker channel summaries should omit direction prefixes such as TX/RX."));
    expectEqual(table->item(0, 2)->text(),
                QStringLiteral("ReadShared"),
                QStringLiteral("Marker list should show opcode summaries."));
    expectEqual(table->item(0, 3)->text(),
                QStringLiteral("0x1000"),
                QStringLiteral("Marker list should show REQ/SNP address summaries."));
    expectEqual(table->item(1, 3)->text(),
                QString(),
                QStringLiteral("Marker list should leave non-REQ/SNP address summaries blank."));
    expect(table->selectedItems().isEmpty()
               || table->selectedItems().front()->data(Qt::UserRole + 1).toString() == QStringLiteral("beta"),
           QStringLiteral("Initially selected marker should stay selected in the unfiltered table."));

    searchEdit->setText(QStringLiteral("alpha"));
    QApplication::processEvents();
    expectEqual(table->rowCount(),
                1,
                QStringLiteral("Marker search should filter by label."));
    expectEqual(table->item(0, 0)->text(),
                QStringLiteral("Alpha anchor"),
                QStringLiteral("Label search should show the matching marker."));
    expect(table->selectedItems().isEmpty(),
           QStringLiteral("A selected marker hidden by search should clear only the visible table selection."));

    searchEdit->setText(QStringLiteral("BUS RETRY"));
    QApplication::processEvents();
    expectEqual(table->rowCount(),
                1,
                QStringLiteral("Marker search should filter by memo case-insensitively."));
    expectEqual(table->item(0, 0)->text(),
                QStringLiteral("Beta checkpoint"),
                QStringLiteral("Memo search should show the marker whose memo matches."));
    expect(!table->selectedItems().isEmpty(),
           QStringLiteral("A selected marker visible after filtering should remain selected."));

    int selectedSignals = 0;
    QString selectedMarkerId;
    QObject::connect(&widget, &MarkerWidget::markerSelected, &widget, [&](const QString& markerId) {
        ++selectedSignals;
        selectedMarkerId = markerId;
    });
    searchEdit->setText(QStringLiteral("gamma"));
    QApplication::processEvents();
    expectEqual(table->rowCount(),
                1,
                QStringLiteral("Search should switch filtered rows immediately."));
    table->selectRow(0);
    QApplication::processEvents();
    expectEqual(selectedSignals,
                1,
                QStringLiteral("Selecting a filtered marker row should emit markerSelected."));
    expectEqual(selectedMarkerId,
                QStringLiteral("gamma"),
                QStringLiteral("Selecting a filtered marker row should report the row marker id."));

    QString editedLabel;
    QObject::connect(&widget, &MarkerWidget::markerEdited, &widget, [&](const TraceMarker& marker) {
        editedLabel = marker.label;
    });
    table->item(0, 0)->setText(QStringLiteral("Gamma renamed"));
    QApplication::processEvents();
    expectEqual(editedLabel,
                QStringLiteral("Gamma renamed"),
                QStringLiteral("Editing a filtered label row should emit the edited marker."));

    searchEdit->clear();
    QApplication::processEvents();
    expectEqual(table->rowCount(),
                3,
                QStringLiteral("Clearing marker search should restore all markers."));

    widget.testSetStickyMode(true);
    QApplication::processEvents();
    expectEqual(widget.testStickyNoteCount(),
                3,
                QStringLiteral("Sticky mode should show all markers on the All canvas."));
    expectEqual(widget.testStickySummaryText(QStringLiteral("alpha")),
                QStringLiteral("REQ  ReadShared  0x1000"),
                QStringLiteral("Sticky notes should show channel/opcode/address summaries below the name."));
    expect(widget.testStickySummaryFontIsSmaller(QStringLiteral("alpha")),
           QStringLiteral("Sticky note summary font should be smaller than the marker name font."));
    expect(widget.testStickyMemoStartsBelowSummary(QStringLiteral("gamma")),
           QStringLiteral("Sticky memo editor should be laid out below wrapped integrated summary contents."));

    searchEdit->setText(QStringLiteral("retry"));
    QApplication::processEvents();
    expectEqual(widget.testStickyNoteCount(),
                1,
                QStringLiteral("Sticky mode should use the same label/memo search filter."));
    searchEdit->clear();
    QApplication::processEvents();

    int stickyActivations = 0;
    QString stickyActivatedMarkerId;
    QObject::connect(&widget, &MarkerWidget::markerActivated, &widget, [&](const QString& markerId) {
        ++stickyActivations;
        stickyActivatedMarkerId = markerId;
        widget.setMarkers(markers, markerId, widget.stickyState(), summaries);
    });
    expect(widget.testDoubleClickStickyNote(QStringLiteral("alpha")),
           QStringLiteral("Double-clicking a sticky note should be handled by the sticky scene item."));
    QApplication::processEvents();
    expectEqual(stickyActivations,
                1,
                QStringLiteral("Double-clicking a sticky note should emit one activation."));
    expectEqual(stickyActivatedMarkerId,
                QStringLiteral("alpha"),
                QStringLiteral("Sticky note double-click should activate the clicked marker."));
    expectEqual(widget.testStickyNoteCount(),
                3,
                QStringLiteral("Sticky note double-click should survive a synchronous marker-widget refresh."));
    QString deferredMemo;
    QObject::connect(&widget, &MarkerWidget::markerEdited, &widget, [&](const TraceMarker& marker) {
        if (marker.id == QStringLiteral("gamma")) {
            deferredMemo = marker.memo;
            markers[2] = marker;
            widget.setMarkers(markers, marker.id, widget.stickyState(), summaries);
        }
    });
    expect(widget.testCommitStickyMemoText(QStringLiteral("gamma"), QStringLiteral("deferred sticky memo")),
           QStringLiteral("Sticky memo test commit should target an existing sticky note."));
    expect(deferredMemo.isEmpty(),
           QStringLiteral("Sticky memo commits should be deferred until after the graphics-scene event returns."));
    QApplication::processEvents();
    expectEqual(deferredMemo,
                QStringLiteral("deferred sticky memo"),
                QStringLiteral("Deferred sticky memo commit should emit markerEdited on the next event turn."));
    expectEqual(widget.testStickyNoteCount(),
                3,
                QStringLiteral("Deferred sticky memo commit should survive synchronous marker view refresh."));

    expect(widget.testMoveStickyNote(QStringLiteral("beta"), QPointF(88.0, 72.0), QSizeF(222.0, 144.0)),
           QStringLiteral("Sticky note test move should update the layout."));
    CHIron::Gui::MarkerStickyState stickyState = widget.stickyState();
    const auto allGroup = std::find_if(stickyState.groups.cbegin(), stickyState.groups.cend(), [](const CHIron::Gui::MarkerStickyGroup& group) {
        return group.id == QStringLiteral("__all__");
    });
    expect(allGroup != stickyState.groups.cend(),
           QStringLiteral("Sticky state should contain the All group."));
    const auto movedLayout = std::find_if(allGroup->noteLayouts.cbegin(), allGroup->noteLayouts.cend(), [](const CHIron::Gui::MarkerStickyNoteLayout& layout) {
        return layout.markerId == QStringLiteral("beta");
    });
    expect(movedLayout != allGroup->noteLayouts.cend(),
           QStringLiteral("Moved sticky note should have a saved layout."));
    expectNear(static_cast<int>(movedLayout->x),
               88,
               1,
               QStringLiteral("Sticky note x position should be stored."));
    expectNear(static_cast<int>(movedLayout->width),
               222,
               1,
               QStringLiteral("Sticky note width should be stored."));
    expect(widget.testMoveStickyNote(QStringLiteral("alpha"), QPointF(40.0, 40.0), QSizeF(160.0, 120.0)),
           QStringLiteral("Sticky note snap setup should position alpha."));
    expect(widget.testMoveStickyNote(QStringLiteral("beta"), QPointF(260.0, 50.0), QSizeF(160.0, 120.0)),
           QStringLiteral("Sticky note snap setup should position beta."));
    expect(widget.testMoveStickyNote(QStringLiteral("gamma"), QPointF(500.0, 50.0), QSizeF(150.0, 110.0)),
           QStringLiteral("Sticky note snap setup should position gamma."));
    expect(widget.testDragStickyNote(QStringLiteral("gamma"), QPointF(209.0, 53.0)),
           QStringLiteral("Sticky note magnetic drag should target an existing sticky note."));
    stickyState = widget.stickyState();
    const auto snappedEdgeGroup = std::find_if(stickyState.groups.cbegin(), stickyState.groups.cend(), [](const CHIron::Gui::MarkerStickyGroup& group) {
        return group.id == QStringLiteral("__all__");
    });
    const auto snappedEdge = std::find_if(snappedEdgeGroup->noteLayouts.cbegin(), snappedEdgeGroup->noteLayouts.cend(), [](const CHIron::Gui::MarkerStickyNoteLayout& layout) {
        return layout.markerId == QStringLiteral("gamma");
    });
    expect(snappedEdge != snappedEdgeGroup->noteLayouts.cend(),
           QStringLiteral("Magnetically moved sticky note should keep a saved layout."));
    expectNear(static_cast<int>(snappedEdge->x),
               200,
               1,
               QStringLiteral("Sticky note should magnetically dock its left edge to a nearby note right edge."));
    expectNear(static_cast<int>(snappedEdge->y),
               55,
               1,
               QStringLiteral("Sticky note should magnetically align to the nearest vertical edge or center guide while docking."));
    expect(widget.testMoveStickyNote(QStringLiteral("gamma"), QPointF(500.0, 300.0), QSizeF(130.0, 110.0)),
           QStringLiteral("Sticky note center snap setup should use a note width distinct from the target."));
    expect(widget.testDragStickyNote(QStringLiteral("gamma"), QPointF(62.0, 300.0)),
           QStringLiteral("Sticky note center snap drag should target an existing sticky note."));
    stickyState = widget.stickyState();
    const auto centerGroup = std::find_if(stickyState.groups.cbegin(), stickyState.groups.cend(), [](const CHIron::Gui::MarkerStickyGroup& group) {
        return group.id == QStringLiteral("__all__");
    });
    const auto centerSnap = std::find_if(centerGroup->noteLayouts.cbegin(), centerGroup->noteLayouts.cend(), [](const CHIron::Gui::MarkerStickyNoteLayout& layout) {
        return layout.markerId == QStringLiteral("gamma");
    });
    expectNear(static_cast<int>(centerSnap->x),
               55,
               1,
               QStringLiteral("Sticky note should magnetically align centers with nearby notes."));
    expect(widget.testDragStickyNote(QStringLiteral("gamma"), QPointF(226.0, 75.0)),
           QStringLiteral("Sticky note outside magnetic threshold should remain free-form."));
    stickyState = widget.stickyState();
    const auto freeGroup = std::find_if(stickyState.groups.cbegin(), stickyState.groups.cend(), [](const CHIron::Gui::MarkerStickyGroup& group) {
        return group.id == QStringLiteral("__all__");
    });
    const auto freeMove = std::find_if(freeGroup->noteLayouts.cbegin(), freeGroup->noteLayouts.cend(), [](const CHIron::Gui::MarkerStickyNoteLayout& layout) {
        return layout.markerId == QStringLiteral("gamma");
    });
    expectNear(static_cast<int>(freeMove->x),
               226,
               1,
               QStringLiteral("Sticky note should not dock when outside the magnetic threshold."));
    expect(widget.testMoveStickyNote(QStringLiteral("gamma"), QPointF(40.0, 220.0), QSizeF(150.0, 110.0)),
           QStringLiteral("Sticky note resize snap setup should position gamma."));
    expect(widget.testMoveStickyNote(QStringLiteral("beta"), QPointF(260.0, 220.0), QSizeF(160.0, 120.0)),
           QStringLiteral("Sticky note resize snap setup should provide a vertical target."));
    expect(widget.testResizeStickyNote(QStringLiteral("gamma"), QSizeF(164.0, 119.0)),
           QStringLiteral("Sticky note magnetic resize should target an existing sticky note."));
    stickyState = widget.stickyState();
    const auto resizedGroup = std::find_if(stickyState.groups.cbegin(), stickyState.groups.cend(), [](const CHIron::Gui::MarkerStickyGroup& group) {
        return group.id == QStringLiteral("__all__");
    });
    const auto resized = std::find_if(resizedGroup->noteLayouts.cbegin(), resizedGroup->noteLayouts.cend(), [](const CHIron::Gui::MarkerStickyNoteLayout& layout) {
        return layout.markerId == QStringLiteral("gamma");
    });
    expectNear(static_cast<int>(resized->width),
               160,
               1,
               QStringLiteral("Sticky note resize should magnetically dock the right edge to a nearby note edge."));
    expectNear(static_cast<int>(resized->height),
               120,
               1,
               QStringLiteral("Sticky note resize should magnetically dock the bottom edge to a nearby note edge."));
    expect(widget.testDragStickyNote(QStringLiteral("gamma"), QPointF(207.0, 53.0), Qt::AltModifier),
           QStringLiteral("Alt sticky drag should use grid snapping."));
    stickyState = widget.stickyState();
    const auto altDragGroup = std::find_if(stickyState.groups.cbegin(), stickyState.groups.cend(), [](const CHIron::Gui::MarkerStickyGroup& group) {
        return group.id == QStringLiteral("__all__");
    });
    const auto altDrag = std::find_if(altDragGroup->noteLayouts.cbegin(), altDragGroup->noteLayouts.cend(), [](const CHIron::Gui::MarkerStickyNoteLayout& layout) {
        return layout.markerId == QStringLiteral("gamma");
    });
    expectNear(static_cast<int>(altDrag->x),
               208,
               1,
               QStringLiteral("Alt sticky drag should snap x to the 16 px grid instead of magnetic docking."));
    expectNear(static_cast<int>(altDrag->y),
               48,
               1,
               QStringLiteral("Alt sticky drag should snap y to the 16 px grid."));
    expect(widget.testResizeStickyNote(QStringLiteral("gamma"), QSizeF(123.0, 91.0), Qt::AltModifier),
           QStringLiteral("Alt sticky resize should use grid snapping."));
    stickyState = widget.stickyState();
    const auto altResizeGroup = std::find_if(stickyState.groups.cbegin(), stickyState.groups.cend(), [](const CHIron::Gui::MarkerStickyGroup& group) {
        return group.id == QStringLiteral("__all__");
    });
    const auto altResize = std::find_if(altResizeGroup->noteLayouts.cbegin(), altResizeGroup->noteLayouts.cend(), [](const CHIron::Gui::MarkerStickyNoteLayout& layout) {
        return layout.markerId == QStringLiteral("gamma");
    });
    expectNear(static_cast<int>(altResize->width),
               128,
               1,
               QStringLiteral("Alt sticky resize should snap width to the 16 px grid."));
    expectNear(static_cast<int>(altResize->height),
               96,
               1,
               QStringLiteral("Alt sticky resize should snap height to the 16 px grid."));
    expect(widget.testPreviewStickyNoteDrag(QStringLiteral("gamma"), QPointF(211.0, 62.0), Qt::AltModifier),
           QStringLiteral("Previewing an Alt sticky drag should show the alignment grid."));
    expect(widget.testStickyGridVisible(),
           QStringLiteral("Alt sticky drag preview should make the alignment grid visible."));
    widget.testFinishStickyInteraction();
    expect(!widget.testStickyGridVisible(),
           QStringLiteral("Finishing sticky drag/resize should hide the alignment grid."));

    QString stickyEditedLabel;
    QString stickyEditedMemo;
    QObject::connect(&widget, &MarkerWidget::markerEdited, &widget, [&](const TraceMarker& marker) {
        stickyEditedLabel = marker.label;
        stickyEditedMemo = marker.memo;
    });
    expect(widget.testEditStickyNote(QStringLiteral("beta"),
                                     QStringLiteral("Beta inline"),
                                     QStringLiteral("inline sticky memo")),
           QStringLiteral("Sticky inline edit should target an existing marker."));
    expectEqual(stickyEditedLabel,
                QStringLiteral("Beta inline"),
                QStringLiteral("Sticky inline label edit should emit markerEdited."));
    expectEqual(stickyEditedMemo,
                QStringLiteral("inline sticky memo"),
                QStringLiteral("Sticky inline memo edit should emit markerEdited."));

    expect(widget.testSelectStickyGroup(QStringLiteral("__all__")),
           QStringLiteral("Sticky tests should be able to return to the All group."));
    widget.setMarkers(markers, QStringLiteral("beta"), widget.stickyState(), summaries);
    QApplication::processEvents();
    const int selectedSignalsBeforeDrag = selectedSignals;
    expect(widget.testDragStickyNote(QStringLiteral("gamma"), QPointF(320.0, 180.0)),
           QStringLiteral("Sticky drag selection test should target an existing note."));
    expectEqual(widget.testSelectedMarkerId(),
                QStringLiteral("beta"),
                QStringLiteral("Dragging a sticky note should not change the selected marker."));
    expectEqual(selectedSignals,
                selectedSignalsBeforeDrag,
                QStringLiteral("Dragging a sticky note should not emit markerSelected."));
    expect(widget.testResizeStickyNote(QStringLiteral("gamma"), QSizeF(176.0, 128.0)),
           QStringLiteral("Sticky resize selection test should target an existing note."));
    expectEqual(widget.testSelectedMarkerId(),
                QStringLiteral("beta"),
                QStringLiteral("Resizing a sticky note should not change the selected marker."));
    expectEqual(selectedSignals,
                selectedSignalsBeforeDrag,
                QStringLiteral("Resizing a sticky note should not emit markerSelected."));

    expect(widget.testAddStickyGroup(QStringLiteral("Investigation")),
           QStringLiteral("Sticky test group creation should succeed."));
    const QString investigationGroupId = widget.testActiveStickyGroupId();
    expect(!investigationGroupId.isEmpty() && investigationGroupId != QStringLiteral("__all__"),
           QStringLiteral("Custom sticky group should become active."));
    expectEqual(widget.testStickyNoteCount(),
                0,
                QStringLiteral("New custom sticky group should start empty."));
    expect(widget.testAddMarkerToActiveStickyGroup(QStringLiteral("gamma")),
           QStringLiteral("Adding marker to custom sticky group should succeed."));
    QApplication::processEvents();
    expectEqual(widget.testStickyNoteCount(),
                1,
                QStringLiteral("Custom sticky group should show its member marker."));
    expect(widget.testStickyCanDeleteFromGroup(QStringLiteral("gamma")),
           QStringLiteral("Delete from group should be enabled in any custom group."));
    expect(widget.testRemoveMarkerFromActiveStickyGroup(QStringLiteral("gamma")),
           QStringLiteral("Toolbar group removal should allow deleting from a custom group."));
    expectEqual(widget.testStickyNoteCount(),
                0,
                QStringLiteral("A marker should disappear after deletion from the active custom group."));
    expect(widget.testAddMarkerToActiveStickyGroup(QStringLiteral("gamma")),
           QStringLiteral("Adding marker back to custom sticky group should succeed for copy/move tests."));
    QApplication::processEvents();

    expect(widget.testAddStickyGroup(QStringLiteral("Archive")),
           QStringLiteral("Second sticky test group creation should succeed."));
    const QString archiveGroupId = widget.testActiveStickyGroupId();
    expect(!archiveGroupId.isEmpty() && archiveGroupId != investigationGroupId,
           QStringLiteral("Second custom group should have a distinct id."));
    expect(widget.testStickyCopyToGroup(QStringLiteral("gamma"), archiveGroupId),
           QStringLiteral("Copy into group should add marker membership to the target group."));
    expect(widget.testStickyMarkerInGroup(investigationGroupId, QStringLiteral("gamma")),
           QStringLiteral("Copy into group should preserve source group membership."));
    expect(widget.testStickyMarkerInGroup(archiveGroupId, QStringLiteral("gamma")),
           QStringLiteral("Copy into group should create target group membership."));
    expect(widget.testSelectStickyGroup(investigationGroupId),
           QStringLiteral("Sticky tests should be able to select the source group."));
    expect(widget.testStickyCanDeleteFromGroup(QStringLiteral("gamma")),
           QStringLiteral("Delete from group should be enabled when another custom group still contains the marker."));
    expect(widget.testStickyDeleteFromGroup(QStringLiteral("gamma")),
           QStringLiteral("Delete from group should remove only the active custom-group membership."));
    expect(!widget.testStickyMarkerInGroup(investigationGroupId, QStringLiteral("gamma")),
           QStringLiteral("Delete from group should remove the marker from the active custom group."));
    expect(widget.testStickyMarkerInGroup(archiveGroupId, QStringLiteral("gamma")),
           QStringLiteral("Delete from group should preserve other custom group memberships."));
    expect(widget.testSelectStickyGroup(archiveGroupId),
           QStringLiteral("Sticky tests should be able to select the remaining gamma group."));
    expect(widget.testStickyCanDeleteFromGroup(QStringLiteral("gamma")),
           QStringLiteral("Delete from group should stay enabled with one custom membership left."));
    expect(widget.testSelectStickyGroup(QStringLiteral("__all__")),
           QStringLiteral("Sticky tests should be able to select All again."));
    expect(!widget.testStickyCanDeleteFromGroup(QStringLiteral("gamma")),
           QStringLiteral("Delete from group should be disabled on All."));

    expect(widget.testStickyCopyToGroup(QStringLiteral("beta"), archiveGroupId),
           QStringLiteral("Copy into an existing group should support selected and non-selected markers."));
    expect(widget.testStickyMoveToGroup(QStringLiteral("beta"), investigationGroupId),
           QStringLiteral("Move from All should behave like copy into the target group."));
    expect(widget.testStickyMarkerInGroup(archiveGroupId, QStringLiteral("beta")),
           QStringLiteral("Move from All should not remove existing custom group membership."));
    expect(widget.testStickyMarkerInGroup(investigationGroupId, QStringLiteral("beta")),
           QStringLiteral("Move from All should add the target custom group membership."));
    expect(widget.testSelectStickyGroup(archiveGroupId),
           QStringLiteral("Sticky tests should be able to select the archive group."));
    expect(widget.testStickyMoveToGroup(QStringLiteral("beta"), investigationGroupId),
           QStringLiteral("Move from a custom group should move membership to the target group."));
    expect(!widget.testStickyMarkerInGroup(archiveGroupId, QStringLiteral("beta")),
           QStringLiteral("Move from a custom group should remove source membership."));
    expect(widget.testStickyMarkerInGroup(investigationGroupId, QStringLiteral("beta")),
           QStringLiteral("Move from a custom group should preserve target membership."));

    expect(widget.testStickyCopyToNewGroup(QStringLiteral("alpha"), QStringLiteral("Copied Notes")),
           QStringLiteral("Copy into new group should create and activate a custom group."));
    const QString copiedGroupId = widget.testActiveStickyGroupId();
    expect(widget.testStickyMarkerInGroup(copiedGroupId, QStringLiteral("alpha")),
           QStringLiteral("Copy into new group should add marker membership to the new group."));
    expect(widget.testStickyMoveToNewGroup(QStringLiteral("alpha"), QStringLiteral("Moved Notes")),
           QStringLiteral("Move into new group should create and activate a custom group."));
    const QString movedGroupId = widget.testActiveStickyGroupId();
    expect(widget.testStickyMarkerInGroup(movedGroupId, QStringLiteral("alpha")),
           QStringLiteral("Move into new group should add marker membership to the new group."));
    expect(!widget.testStickyMarkerInGroup(copiedGroupId, QStringLiteral("alpha")),
           QStringLiteral("Move into new group should remove source custom group membership."));

    int stickyRemovedSignals = 0;
    QString stickyRemovedMarkerId;
    QObject::connect(&widget, &MarkerWidget::markerRemoved, &widget, [&](const QString& markerId) {
        ++stickyRemovedSignals;
        stickyRemovedMarkerId = markerId;
    });
    expect(widget.testStickyJumpToRow(QStringLiteral("gamma")),
           QStringLiteral("Sticky context jump should target an existing marker."));
    QApplication::processEvents();
    expectEqual(stickyActivatedMarkerId,
                QStringLiteral("gamma"),
                QStringLiteral("Sticky context Jump to row should emit markerActivated."));
    expect(widget.testStickyDeleteMarker(QStringLiteral("gamma")),
           QStringLiteral("Sticky context Delete marker should target an existing marker."));
    expectEqual(stickyRemovedSignals,
                1,
                QStringLiteral("Sticky context Delete marker should emit markerRemoved."));
    expectEqual(stickyRemovedMarkerId,
                QStringLiteral("gamma"),
                QStringLiteral("Sticky context Delete marker should report the marker id."));

    expect(widget.testSelectStickyGroup(QStringLiteral("__all__")),
           QStringLiteral("Sticky Delete shortcut test should run with the selected note visible."));
    widget.setMarkers(markers, QStringLiteral("beta"), widget.stickyState(), summaries);
    widget.testSetStickyMode(true);
    QApplication::processEvents();
    widget.testSetStickyTextEditing(true);
    expect(!widget.testTriggerStickyDeleteShortcut(),
           QStringLiteral("Delete shortcut should not remove a sticky note while inline text editing has focus."));
    expectEqual(stickyRemovedSignals,
                1,
                QStringLiteral("Blocked Delete shortcut should not emit markerRemoved."));
    widget.testSetStickyTextEditing(false);
    expect(widget.testTriggerStickyDeleteShortcut(),
           QStringLiteral("Delete shortcut should remove the selected sticky note outside inline editing."));
    expectEqual(stickyRemovedSignals,
                2,
                QStringLiteral("Delete shortcut should emit markerRemoved once."));
    expectEqual(stickyRemovedMarkerId,
                QStringLiteral("beta"),
                QStringLiteral("Delete shortcut should remove the selected marker."));
}

void testMarkerJsonPersistsStickyState()
{
    using CHIron::Gui::LoadTraceMarkersFromJson;
    using CHIron::Gui::MarkerStickyGroup;
    using CHIron::Gui::MarkerStickyNoteLayout;
    using CHIron::Gui::MarkerStickyState;
    using CHIron::Gui::SaveTraceMarkersToJson;
    using CHIron::Gui::TraceMarker;
    using CHIron::Gui::TraceMarkerLoadResult;

    TraceMarker markerA;
    markerA.id = QStringLiteral("marker-a");
    markerA.logicalRow = 0;
    markerA.timestamp = 100;
    markerA.label = QStringLiteral("A");
    markerA.color = QColor(QStringLiteral("#3366cc"));

    TraceMarker markerB;
    markerB.id = QStringLiteral("marker-b");
    markerB.logicalRow = 1;
    markerB.timestamp = 200;
    markerB.label = QStringLiteral("B");
    markerB.color = QColor(QStringLiteral("#00aa88"));

    MarkerStickyGroup group;
    group.id = QStringLiteral("group-1");
    group.name = QStringLiteral("Investigate");
    group.markerIds = {markerB.id, QStringLiteral("missing-marker")};
    MarkerStickyNoteLayout layout;
    layout.markerId = markerB.id;
    layout.x = 42.0;
    layout.y = 64.0;
    layout.width = 210.0;
    layout.height = 130.0;
    group.noteLayouts.push_back(layout);
    MarkerStickyNoteLayout invalidLayout;
    invalidLayout.markerId = QStringLiteral("missing-marker");
    group.noteLayouts.push_back(invalidLayout);

    MarkerStickyState state;
    state.activeGroupId = group.id;
    state.groups.push_back(group);

    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Sticky marker JSON test temporary directory should be valid."));
    const QString path = tempDir.filePath(QStringLiteral("sticky.markers.json"));
    QString error;
    expect(SaveTraceMarkersToJson(path,
                                  std::vector<TraceMarker>{markerA, markerB},
                                  state,
                                  QStringLiteral("trace.clogb"),
                                  QStringLiteral("trace"),
                                  error),
           error.isEmpty() ? QStringLiteral("Sticky marker JSON should save.")
                           : error);

    TraceMarkerLoadResult result;
    expect(LoadTraceMarkersFromJson(path, 2, result, error),
           error.isEmpty() ? QStringLiteral("Sticky marker JSON should load.")
                           : error);
    expect(result.stickyState.has_value(),
           QStringLiteral("Loading sticky marker JSON should return sticky state."));
    expectEqual(result.stickyState->activeGroupId,
                QStringLiteral("group-1"),
                QStringLiteral("Sticky active group should round-trip."));
    expectEqual(static_cast<int>(result.stickyState->groups.size()),
                1,
                QStringLiteral("Sticky custom groups should round-trip."));
    expectEqual(static_cast<int>(result.stickyState->groups.front().markerIds.size()),
                1,
                QStringLiteral("Unknown marker ids should be skipped from sticky group membership."));
    expectEqual(result.stickyState->groups.front().markerIds.front(),
                markerB.id,
                QStringLiteral("Valid sticky group marker membership should round-trip."));
    expectEqual(static_cast<int>(result.stickyState->groups.front().noteLayouts.size()),
                1,
                QStringLiteral("Unknown marker ids should be skipped from sticky note layouts."));
    expectNear(static_cast<int>(result.stickyState->groups.front().noteLayouts.front().width),
               210,
               1,
               QStringLiteral("Sticky note width should round-trip."));

    const QString oldPath = tempDir.filePath(QStringLiteral("old.markers.json"));
    expect(SaveTraceMarkersToJson(oldPath,
                                  std::vector<TraceMarker>{markerA},
                                  QStringLiteral("trace.clogb"),
                                  QStringLiteral("trace"),
                                  error),
           error.isEmpty() ? QStringLiteral("Legacy marker JSON overload should save.")
                           : error);
    TraceMarkerLoadResult oldResult;
    expect(LoadTraceMarkersFromJson(oldPath, 1, oldResult, error),
           error.isEmpty() ? QStringLiteral("Legacy marker JSON overload should load.")
                           : error);
    expect(oldResult.stickyState.has_value(),
           QStringLiteral("Saved marker JSON should include an optional sticky block."));
}

void testMainWindowPreservesPerSessionTableDetails()
{
    std::vector<CHIron::Gui::FlitRecord> firstRows = buildOptionalFieldStateRows(80);
    std::vector<CHIron::Gui::FlitRecord> secondRows = buildOptionalFieldStateRows(80);
    for (CHIron::Gui::FlitRecord& row : secondRows) {
        row.opcode = QStringLiteral("SecondOnly");
        row.fields.front().value = QStringLiteral("9");
        row.fields.front().raw = QStringLiteral("0x9");
    }

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testApplyTraceRows(std::move(firstRows), QStringLiteral("table_state_a.clogb")),
           QStringLiteral("First table-state session should open."));
    window.testSetFieldColumnVisible(QStringLiteral("QoS"), true);
    window.testSetFieldFilter(QStringLiteral("QoS"), QStringLiteral("7"));
    window.testSetSearchMode(CHIron::Gui::FlitTableModel::SearchMode::Highlight);
    const int qosColumn = window.testFieldColumn(QStringLiteral("QoS"));
    expect(qosColumn >= CHIron::Gui::FlitTableModel::ColumnCount,
           QStringLiteral("Visible optional field should have a table column."));
    window.testSetDisplayMode(qosColumn, CHIron::Gui::FlitTableModel::DisplayMode::Hexadecimal);
    window.testSetDisplayMode(CHIron::Gui::FlitTableModel::TimeColumn,
                              CHIron::Gui::FlitTableModel::DisplayMode::Decimal);
    window.testSelectLogicalRow(2);
    window.testSetTableColumnWidth(CHIron::Gui::FlitTableModel::OpcodeColumn, 217);
    window.testSetTableScrollValue(12);

    expect(window.testApplyTraceRows(std::move(secondRows), QStringLiteral("table_state_b.clogb")),
           QStringLiteral("Second table-state session should open."));
    expect(!window.testFieldColumnVisible(QStringLiteral("QoS")),
           QStringLiteral("Second session should not inherit first session optional columns."));
    expectEqual(static_cast<int>(window.testSearchMode()),
                static_cast<int>(CHIron::Gui::FlitTableModel::SearchMode::Filter),
                QStringLiteral("Second session should keep default search mode."));

    window.testSetOpcodeFilter(QStringLiteral("SecondOnly"));
    window.testSelectLogicalRow(1);

    expect(window.testSwitchToSession(0),
           QStringLiteral("Switching back to first table-state session should succeed."));
    QApplication::processEvents();
    expect(window.testFieldColumnVisible(QStringLiteral("QoS")),
           QStringLiteral("First session optional field column should be restored."));
    expectEqual(window.testFieldFilter(QStringLiteral("QoS")),
                QStringLiteral("7"),
                QStringLiteral("First session optional field filter should be restored."));
    expectEqual(static_cast<int>(window.testSearchMode()),
                static_cast<int>(CHIron::Gui::FlitTableModel::SearchMode::Highlight),
                QStringLiteral("First session search/highlight mode should be restored."));
    expectEqual(static_cast<int>(window.testDisplayMode(CHIron::Gui::FlitTableModel::TimeColumn)),
                static_cast<int>(CHIron::Gui::FlitTableModel::DisplayMode::Decimal),
                QStringLiteral("First session base-column display mode should be restored."));
    expectEqual(static_cast<int>(window.testDisplayMode(window.testFieldColumn(QStringLiteral("QoS")))),
                static_cast<int>(CHIron::Gui::FlitTableModel::DisplayMode::Hexadecimal),
                QStringLiteral("First session optional-field display mode should be restored."));
    expectEqual(window.testSelectedLogicalRow(),
                2,
                QStringLiteral("First session selected logical row should be restored."));
    expectEqual(window.testTableColumnWidth(CHIron::Gui::FlitTableModel::OpcodeColumn),
                217,
                QStringLiteral("First session table column width should be restored."));
    expectEqual(window.testTableScrollValue(),
                12,
                QStringLiteral("First session table scroll position should be restored."));

    expect(window.testSwitchToSession(1),
           QStringLiteral("Switching back to second table-state session should succeed."));
    QApplication::processEvents();
    expectEqual(window.testOpcodeFilter(),
                QStringLiteral("SecondOnly"),
                QStringLiteral("Second session opcode filter should remain independent."));
    expectEqual(window.testSelectedLogicalRow(),
                1,
                QStringLiteral("Second session selected logical row should remain independent."));
    expect(!window.testFieldColumnVisible(QStringLiteral("QoS")),
           QStringLiteral("Second session optional field visibility should remain independent."));
}

void testMainWindowPreservesPerSessionWidgetState()
{
    std::vector<CHIron::Gui::FlitRecord> firstRows = buildLatencyWidgetTestRows();
    std::vector<CHIron::Gui::FlitRecord> secondRows = buildLatencyWidgetTestRows();
    for (CHIron::Gui::FlitRecord& row : secondRows) {
        row.timestamp += 1000;
        row.opcode = QStringLiteral("SecondStateOnly");
    }

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testApplyTraceRows(std::move(firstRows), QStringLiteral("state_a.clogb")),
           QStringLiteral("Main window should accept the first state test session."));
    window.testBuildVisualizationViews();
    QApplication::processEvents();

    QVariantMap firstTimelineState = window.testTimelineViewState();
    firstTimelineState.insert(QStringLiteral("horizontalZoom"), 4.0);
    firstTimelineState.insert(QStringLiteral("scrollOffset"), QVariant::fromValue<qulonglong>(12));
    firstTimelineState.insert(QStringLiteral("selectedLogicalRow"), 2);
    firstTimelineState.remove(QStringLiteral("visibleFirstTimestamp"));
    firstTimelineState.remove(QStringLiteral("visibleLastTimestamp"));
    firstTimelineState.remove(QStringLiteral("visibleFirstLogicalRow"));
    firstTimelineState.remove(QStringLiteral("visibleLastLogicalRow"));
    firstTimelineState.remove(QStringLiteral("hasTimestampRange"));
    window.testRestoreTimelineViewState(firstTimelineState);

    QVariantMap firstAddressState = window.testAddressViewState();
    firstAddressState.insert(QStringLiteral("horizontalZoom"), 3.0);
    firstAddressState.insert(QStringLiteral("scrollOffset"), QVariant::fromValue<qulonglong>(9));
    firstAddressState.insert(QStringLiteral("selectedLogicalRow"), 1);
    window.testRestoreAddressViewState(firstAddressState);

    QVariantMap firstLatencyState = window.testLatencyViewState();
    firstLatencyState.insert(QStringLiteral("plotType"), QStringLiteral("Violin"));
    firstLatencyState.insert(QStringLiteral("plotZoom"), 2.0);
    firstLatencyState.insert(QStringLiteral("plotCenter"), 15.0);
    firstLatencyState.insert(QStringLiteral("plotHasCursor"), true);
    firstLatencyState.insert(QStringLiteral("plotCursorValue"), 20.0);
    window.testRestoreLatencyViewState(firstLatencyState);

    expect(window.testApplyTraceRows(std::move(secondRows), QStringLiteral("state_b.clogb")),
           QStringLiteral("Main window should accept the second state test session."));
    window.testBuildVisualizationViews();
    QApplication::processEvents();
    QVariantMap secondLatencyState = window.testLatencyViewState();
    secondLatencyState.insert(QStringLiteral("plotType"), QStringLiteral("Jitter"));
    secondLatencyState.insert(QStringLiteral("plotZoom"), 2.0);
    secondLatencyState.insert(QStringLiteral("plotCenter"), 20.0);
    secondLatencyState.insert(QStringLiteral("plotHasCursor"), false);
    window.testRestoreLatencyViewState(secondLatencyState);

    expect(window.testSwitchToSession(0),
           QStringLiteral("Switching back to the first state session should succeed."));
    QApplication::processEvents();
    expectNear(window.testTimelineViewState().value(QStringLiteral("horizontalZoom")).toDouble(),
               4.0,
               0.1,
               QStringLiteral("Timeline horizontal zoom should be restored per session."));
    expectEqual(window.testTimelineViewState().value(QStringLiteral("selectedLogicalRow")).toInt(),
                2,
                QStringLiteral("Timeline cursor row should be restored per session."));
    expectNear(window.testAddressViewState().value(QStringLiteral("horizontalZoom")).toDouble(),
               3.0,
               0.0001,
               QStringLiteral("Address horizontal zoom should be restored per session."));
    expectEqual(window.testLatencyViewState().value(QStringLiteral("plotType")).toString(),
                QStringLiteral("Violin"),
                QStringLiteral("Latency plot type should be restored per session."));
    expectNear(window.testLatencyViewState().value(QStringLiteral("plotCenter")).toDouble(),
               15.0,
               0.0001,
               QStringLiteral("Latency global plot center should be restored per session."));
    expect(window.testLatencyViewState().value(QStringLiteral("plotHasCursor")).toBool(),
           QStringLiteral("Latency cursor visibility should be restored per session."));
    expectNear(window.testLatencyViewState().value(QStringLiteral("plotCursorValue")).toDouble(),
               20.0,
               0.0001,
               QStringLiteral("Latency cursor value should be restored per session."));

    expect(window.testSwitchToSession(1),
           QStringLiteral("Switching back to the second state session should succeed."));
    QApplication::processEvents();
    expectEqual(window.testLatencyViewState().value(QStringLiteral("plotType")).toString(),
                QStringLiteral("Jitter"),
                QStringLiteral("The second session should keep its own latency plot type."));
    expectNear(window.testLatencyViewState().value(QStringLiteral("plotCenter")).toDouble(),
               20.0,
               0.0001,
               QStringLiteral("The second session should keep its own latency global scale center."));
    expect(!window.testLatencyViewState().value(QStringLiteral("plotHasCursor")).toBool(),
           QStringLiteral("The second session should keep its own latency cursor state."));
}

void testMainWindowSessionSwitchKeepsTimelineAndAddressStable()
{
    std::vector<CHIron::Gui::FlitRecord> firstRows = buildTimelineTestRows();
    std::vector<CHIron::Gui::FlitRecord> secondRows = buildSecondSessionSwitchRows();

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testApplyTraceRows(std::move(firstRows), QStringLiteral("switch_stable_a.clogb")),
           QStringLiteral("First switch-stability session should open."));
    window.testBuildVisualizationViews();
    QApplication::processEvents();
    const int firstTimelineWidgetId = window.testTimelineWidgetInstanceId();
    const int firstAddressWidgetId = window.testAddressWidgetInstanceId();

    expect(window.testApplyTraceRows(std::move(secondRows), QStringLiteral("switch_stable_b.clogb")),
           QStringLiteral("Second switch-stability session should open."));
    window.testBuildVisualizationViews();
    QApplication::processEvents();
    expect(window.testTimelineWidgetInstanceId() != firstTimelineWidgetId,
           QStringLiteral("Second session should use a separate Timeline widget instance."));
    expect(window.testAddressWidgetInstanceId() != firstAddressWidgetId,
           QStringLiteral("Second session should use a separate Address widget instance."));

    QVariantMap secondTimelineState = window.testTimelineViewState();
    secondTimelineState.insert(QStringLiteral("horizontalZoom"), 4.0);
    secondTimelineState.insert(QStringLiteral("scrollOffset"), QVariant::fromValue<qulonglong>(240));
    window.testRestoreTimelineViewState(secondTimelineState);

    QVariantMap secondAddressState = window.testAddressViewState();
    secondAddressState.insert(QStringLiteral("horizontalZoom"), 3.0);
    secondAddressState.insert(QStringLiteral("scrollOffset"), QVariant::fromValue<qulonglong>(180));
    window.testRestoreAddressViewState(secondAddressState);

    const int secondTimelineWidgetId = window.testTimelineWidgetInstanceId();
    const int secondAddressWidgetId = window.testAddressWidgetInstanceId();
    const QString secondTimelineRange = window.testTimelineVisibleRangeText();
    const QString secondAddressRange = window.testAddressVisibleRangeText();
    expect(!secondTimelineRange.isEmpty(),
           QStringLiteral("Second session Timeline range text should be available."));
    expect(!secondAddressRange.isEmpty(),
           QStringLiteral("Second session Address range text should be available."));

    for (int iteration = 0; iteration < 5; ++iteration) {
        expect(window.testSwitchToSession(0),
               QStringLiteral("Switching to the first session should succeed."));
        QApplication::processEvents();
        expectEqual(window.testTimelineWidgetInstanceId(),
                    firstTimelineWidgetId,
                    QStringLiteral("First session Timeline widget should be reused."));
        expectEqual(window.testAddressWidgetInstanceId(),
                    firstAddressWidgetId,
                    QStringLiteral("First session Address widget should be reused."));

        expect(window.testSwitchToSession(1),
               QStringLiteral("Switching back to the second session should succeed."));
        QApplication::processEvents();
        expectEqual(window.testTimelineWidgetInstanceId(),
                    secondTimelineWidgetId,
                    QStringLiteral("Second session Timeline widget should be reused on every switch."));
        expectEqual(window.testAddressWidgetInstanceId(),
                    secondAddressWidgetId,
                    QStringLiteral("Second session Address widget should be reused on every switch."));
        expectEqual(window.testTimelineVisibleRangeText(),
                    secondTimelineRange,
                    QStringLiteral("Second session Timeline contents should stay stable across switches."));
        expectEqual(window.testAddressVisibleRangeText(),
                    secondAddressRange,
                    QStringLiteral("Second session Address contents should stay stable across switches."));
    }
}

void testMainWindowTransactionWidgetInstancesAndStateArePerSession()
{
    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testApplyTraceRows(buildTimelineTestRows(), QStringLiteral("transaction_state_a.clogb")),
           QStringLiteral("First transaction-widget state session should open."));
    window.testInjectTransactionSpans(2, 12);
    QVariantMap firstState = window.testTransactionViewState();
    firstState.insert(QStringLiteral("horizontalZoom"), 3.0);
    firstState.insert(QStringLiteral("scrollOffset"), QVariant::fromValue<qulonglong>(24));
    firstState.insert(QStringLiteral("verticalScrollOffset"), 1);
    firstState.insert(QStringLiteral("selectedLogicalRow"), 6);
    firstState.insert(QStringLiteral("selectedSpanIndex"), 2);
    firstState.insert(QStringLiteral("hasCursor"), true);
    firstState.insert(QStringLiteral("cursorTimestamp"), 55);
    window.testRestoreTransactionViewState(firstState);
    const int firstWidgetId = window.testTransactionWidgetInstanceId();
    const QString firstRangeText = window.testTransactionVisibleRangeText();

    expect(window.testApplyTraceRows(buildSecondSessionSwitchRows(), QStringLiteral("transaction_state_b.clogb")),
           QStringLiteral("Second transaction-widget state session should open."));
    QApplication::processEvents();
    expect(window.testTransactionWidgetInstanceId() != firstWidgetId,
           QStringLiteral("Second session should use a separate Transaction widget instance."));
    window.testInjectTransactionSpans(3, 21);
    QVariantMap secondState = window.testTransactionViewState();
    secondState.insert(QStringLiteral("horizontalZoom"), 5.0);
    secondState.insert(QStringLiteral("scrollOffset"), QVariant::fromValue<qulonglong>(72));
    secondState.insert(QStringLiteral("verticalScrollOffset"), 2);
    secondState.insert(QStringLiteral("selectedLogicalRow"), 15);
    secondState.insert(QStringLiteral("selectedSpanIndex"), 5);
    secondState.insert(QStringLiteral("hasCursor"), true);
    secondState.insert(QStringLiteral("cursorTimestamp"), 111);
    window.testRestoreTransactionViewState(secondState);
    const int secondWidgetId = window.testTransactionWidgetInstanceId();
    const QString secondRangeText = window.testTransactionVisibleRangeText();

    expect(window.testSwitchToSession(0),
           QStringLiteral("Switching back to first transaction-widget session should succeed."));
    QApplication::processEvents();
    expectEqual(window.testTransactionWidgetInstanceId(),
                firstWidgetId,
                QStringLiteral("First session should reuse its original Transaction widget instance."));
    expectEqual(window.testTransactionSpanCount(),
                12,
                QStringLiteral("First session transaction cache should survive session switching."));
    expectEqual(window.testTransactionLaneCount(),
                2,
                QStringLiteral("First session transaction stack rows should survive session switching."));
    expectNear(window.testTransactionViewState().value(QStringLiteral("horizontalZoom")).toDouble(),
               3.0,
               0.0001,
               QStringLiteral("First session transaction zoom should be restored."));
    expectEqual(window.testTransactionViewState().value(QStringLiteral("selectedLogicalRow")).toInt(),
                6,
                QStringLiteral("First session transaction selection should be restored."));
    expectEqual(window.testTransactionVisibleRangeText(),
                firstRangeText,
                QStringLiteral("First session transaction visible range should remain stable."));

    expect(window.testSwitchToSession(1),
           QStringLiteral("Switching back to second transaction-widget session should succeed."));
    QApplication::processEvents();
    expectEqual(window.testTransactionWidgetInstanceId(),
                secondWidgetId,
                QStringLiteral("Second session should reuse its original Transaction widget instance."));
    expectEqual(window.testTransactionSpanCount(),
                21,
                QStringLiteral("Second session transaction cache should survive session switching."));
    expectEqual(window.testTransactionLaneCount(),
                3,
                QStringLiteral("Second session transaction stack rows should survive session switching."));
    expectNear(window.testTransactionViewState().value(QStringLiteral("horizontalZoom")).toDouble(),
               5.0,
               0.0001,
               QStringLiteral("Second session transaction zoom should be restored."));
    expectEqual(window.testTransactionViewState().value(QStringLiteral("selectedLogicalRow")).toInt(),
                15,
                QStringLiteral("Second session transaction selection should be restored."));
    expectEqual(window.testTransactionVisibleRangeText(),
                secondRangeText,
                QStringLiteral("Second session transaction visible range should remain stable."));
}

void testMainWindowClosingSessionDoesNotClearActiveTransactionWidget()
{
    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testApplyTraceRows(buildTimelineTestRows(), QStringLiteral("transaction_close_a.clogb")),
           QStringLiteral("First transaction close test session should open."));
    window.testInjectTransactionSpans(2, 10);
    const int firstWidgetId = window.testTransactionWidgetInstanceId();

    expect(window.testApplyTraceRows(buildSecondSessionSwitchRows(), QStringLiteral("transaction_close_b.clogb")),
           QStringLiteral("Second transaction close test session should open."));
    window.testInjectTransactionSpans(4, 16);
    const int activeWidgetId = window.testTransactionWidgetInstanceId();
    const QString activeRangeText = window.testTransactionVisibleRangeText();

    expect(window.testCloseSession(0),
           QStringLiteral("Closing inactive first session should succeed."));
    QApplication::processEvents();
    expectEqual(window.testSessionCount(),
                1,
                QStringLiteral("One session should remain after closing the inactive transaction session."));
    expectEqual(window.testTransactionWidgetInstanceId(),
                activeWidgetId,
                QStringLiteral("Closing an inactive session must not replace the active Transaction widget."));
    expect(window.testTransactionWidgetInstanceId() != firstWidgetId,
           QStringLiteral("The closed session's Transaction widget should not remain active."));
    expectEqual(window.testTransactionSpanCount(),
                16,
                QStringLiteral("Closing an inactive session must not clear the active Transaction cache."));
    expectEqual(window.testTransactionVisibleRangeText(),
                activeRangeText,
                QStringLiteral("Closing an inactive session must not shift the active Transaction range."));
}

void testMainWindowCacheWidgetInstancesAndStateArePerSession()
{
    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testApplyTraceRows(buildTimelineTestRows(), QStringLiteral("cache_state_a.clogb")),
           QStringLiteral("First cache-widget state session should open."));
    window.testInjectCacheSpans(2, 12);
    QVariantMap firstState = window.testCacheViewState();
    firstState.insert(QStringLiteral("horizontalZoom"), 3.0);
    firstState.insert(QStringLiteral("scrollOffset"), QVariant::fromValue<qulonglong>(24));
    firstState.insert(QStringLiteral("verticalScrollOffset"), 1);
    firstState.insert(QStringLiteral("selectedLogicalRow"), 6);
    firstState.insert(QStringLiteral("selectedSpanIndex"), 2);
    firstState.insert(QStringLiteral("hasCursor"), true);
    firstState.insert(QStringLiteral("cursorTimestamp"), 48);
    window.testRestoreCacheViewState(firstState);
    const int firstWidgetId = window.testCacheWidgetInstanceId();
    const QString firstRangeText = window.testCacheVisibleRangeText();

    expect(window.testApplyTraceRows(buildSecondSessionSwitchRows(), QStringLiteral("cache_state_b.clogb")),
           QStringLiteral("Second cache-widget state session should open."));
    QApplication::processEvents();
    expect(window.testCacheWidgetInstanceId() != firstWidgetId,
           QStringLiteral("Second session should use a separate Cache widget instance."));
    window.testInjectCacheSpans(3, 21);
    QVariantMap secondState = window.testCacheViewState();
    secondState.insert(QStringLiteral("horizontalZoom"), 5.0);
    secondState.insert(QStringLiteral("scrollOffset"), QVariant::fromValue<qulonglong>(72));
    secondState.insert(QStringLiteral("verticalScrollOffset"), 2);
    secondState.insert(QStringLiteral("selectedLogicalRow"), 15);
    secondState.insert(QStringLiteral("selectedSpanIndex"), 5);
    secondState.insert(QStringLiteral("hasCursor"), true);
    secondState.insert(QStringLiteral("cursorTimestamp"), 120);
    window.testRestoreCacheViewState(secondState);
    const int secondWidgetId = window.testCacheWidgetInstanceId();
    const QString secondRangeText = window.testCacheVisibleRangeText();

    expect(window.testSwitchToSession(0),
           QStringLiteral("Switching back to first cache-widget session should succeed."));
    QApplication::processEvents();
    expectEqual(window.testCacheWidgetInstanceId(),
                firstWidgetId,
                QStringLiteral("First session should reuse its original Cache widget instance."));
    expectEqual(window.testCacheSpanCount(),
                12,
                QStringLiteral("First session cache lifetimes should survive session switching."));
    expectEqual(window.testCacheBandCount(),
                2,
                QStringLiteral("First session Cache RN bands should survive session switching."));
    expectNear(window.testCacheViewState().value(QStringLiteral("horizontalZoom")).toDouble(),
               3.0,
               0.0001,
               QStringLiteral("First session Cache zoom should be restored."));
    expectEqual(window.testCacheViewState().value(QStringLiteral("selectedLogicalRow")).toInt(),
                6,
                QStringLiteral("First session Cache selection should be restored."));
    expectEqual(window.testCacheVisibleRangeText(),
                firstRangeText,
                QStringLiteral("First session Cache visible range should remain stable."));

    expect(window.testSwitchToSession(1),
           QStringLiteral("Switching back to second cache-widget session should succeed."));
    QApplication::processEvents();
    expectEqual(window.testCacheWidgetInstanceId(),
                secondWidgetId,
                QStringLiteral("Second session should reuse its original Cache widget instance."));
    expectEqual(window.testCacheSpanCount(),
                21,
                QStringLiteral("Second session cache lifetimes should survive session switching."));
    expectEqual(window.testCacheBandCount(),
                3,
                QStringLiteral("Second session Cache RN bands should survive session switching."));
    expectNear(window.testCacheViewState().value(QStringLiteral("horizontalZoom")).toDouble(),
               5.0,
               0.0001,
               QStringLiteral("Second session Cache zoom should be restored."));
    expectEqual(window.testCacheViewState().value(QStringLiteral("selectedLogicalRow")).toInt(),
                15,
                QStringLiteral("Second session Cache selection should be restored."));
    expectEqual(window.testCacheVisibleRangeText(),
                secondRangeText,
                QStringLiteral("Second session Cache visible range should remain stable."));
}

void testMainWindowClosingSessionDoesNotClearActiveCacheWidget()
{
    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testApplyTraceRows(buildTimelineTestRows(), QStringLiteral("cache_close_a.clogb")),
           QStringLiteral("First cache close test session should open."));
    window.testInjectCacheSpans(2, 10);
    const int firstWidgetId = window.testCacheWidgetInstanceId();

    expect(window.testApplyTraceRows(buildSecondSessionSwitchRows(), QStringLiteral("cache_close_b.clogb")),
           QStringLiteral("Second cache close test session should open."));
    window.testInjectCacheSpans(4, 16);
    const int activeWidgetId = window.testCacheWidgetInstanceId();
    const QString activeRangeText = window.testCacheVisibleRangeText();

    expect(window.testCloseSession(0),
           QStringLiteral("Closing inactive first session should succeed."));
    QApplication::processEvents();
    expectEqual(window.testSessionCount(),
                1,
                QStringLiteral("One session should remain after closing the inactive cache session."));
    expectEqual(window.testCacheWidgetInstanceId(),
                activeWidgetId,
                QStringLiteral("Closing an inactive session must not replace the active Cache widget."));
    expect(window.testCacheWidgetInstanceId() != firstWidgetId,
           QStringLiteral("The closed session's Cache widget should not remain active."));
    expectEqual(window.testCacheSpanCount(),
                16,
                QStringLiteral("Closing an inactive session must not clear the active Cache lifetimes."));
    expectEqual(window.testCacheVisibleRangeText(),
                activeRangeText,
                QStringLiteral("Closing an inactive session must not shift the active Cache range."));
}

void testMainWindowCloseSessionWithActiveTransactionBuildIgnoresLateCompletion()
{
    using XactTestConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 256, true, true, true>;
    using ReqFlit = CHI::Eb::Flits::REQ<XactTestConfig>;
    using DatFlit = CHI::Eb::Flits::DAT<XactTestConfig>;

    CLog::Parameters params;
    params.SetIssue(CLog::Issue::Eb);
    expect(params.SetNodeIdWidth(11), QStringLiteral("Failed to set test NodeIDWidth."));
    expect(params.SetReqAddrWidth(52), QStringLiteral("Failed to set test ReqAddrWidth."));
    expect(params.SetReqRSVDCWidth(32), QStringLiteral("Failed to set test ReqRSVDCWidth."));
    expect(params.SetDatRSVDCWidth(32), QStringLiteral("Failed to set test DatRSVDCWidth."));
    expect(params.SetDataWidth(256), QStringLiteral("Failed to set test DataWidth."));
    params.SetDataCheckPresent(true);
    params.SetPoisonPresent(true);
    params.SetMPAMPresent(true);

    const auto serializeReq = [](const ReqFlit& flit, TestFlitBitWriter& writer) {
        appendEbReqFlitForTest(flit, writer);
    };
    const auto serializeDat = [](const DatFlit& flit, TestFlitBitWriter& writer) {
        appendEbDatFlitForTest(flit, writer);
    };

    std::vector<CLog::CLogB::TagCHIRecords::Record> records;
    records.reserve(360);
    for (int index = 0; index < 120; ++index) {
        ReqFlit request{};
        request.TgtID() = 2;
        request.SrcID() = 1;
        request.TxnID() = static_cast<std::uint16_t>(index & 0xFF);
        request.Opcode() = CHI::Eb::Opcodes::REQ::ReadNoSnp;
        request.Size() = CHI::Eb::Sizes::B64;
        request.Addr() = 0x1000 + static_cast<std::uint64_t>(index) * 0x100;
        request.AllowRetry() = 1;

        DatFlit data0{};
        data0.TgtID() = 1;
        data0.SrcID() = 2;
        data0.TxnID() = request.TxnID();
        data0.HomeNID() = 2;
        data0.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
        data0.RespErr() = CHI::Eb::RespErrs::OK;
        data0.Resp() = CHI::Eb::Resps::CompData_UC;
        data0.DBID() = static_cast<std::uint16_t>(index & 0xFF);
        data0.DataID() = 0;
        data0.BE() = 0xFFFFFFFFU;

        DatFlit data2 = data0;
        data2.DataID() = 2;

        records.push_back(makeSerializedRecord(CLog::Channel::TXREQ, 1, 1, request, serializeReq));
        records.push_back(makeSerializedRecord(CLog::Channel::RXDAT, 1, 1, data0, serializeDat));
        records.push_back(makeSerializedRecord(CLog::Channel::RXDAT, 1, 1, data2, serializeDat));
    }

    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    const QString tracePath = tempDir.filePath(QStringLiteral("transaction_close_build.clogb"));
    writeTraceWithTopology(tracePath,
                           params,
                           {
                               {CLog::NodeType::RN_F, 1},
                               {CLog::NodeType::HN_F, 2},
                           },
                           std::move(records));

    CHIron::Gui::CLogBTraceLoadError error;
    std::shared_ptr<CHIron::Gui::TraceSession> indexedSession;
    expect(CHIron::Gui::TraceSession::open(tracePath, indexedSession, error),
           error.summary.isEmpty()
               ? QStringLiteral("Trace session should open for close-during-build transaction test.")
               : error.summary);
    expect(indexedSession != nullptr, QStringLiteral("Trace session should be created."));
    expect(indexedSession->buildXactionIndex(error),
           error.summary.isEmpty()
               ? QStringLiteral("Xaction index should prebuild for close-during-build transaction test.")
               : error.summary);
    indexedSession.reset();

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(tracePath),
           QStringLiteral("Pre-indexed transaction build trace should load."));
    expect(window.testTransactionWidgetInstanceId() != 0,
           QStringLiteral("Pre-indexed trace should have an active Transaction widget instance."));
    expect(window.testCloseActiveSession(),
           QStringLiteral("Closing the session during Transaction span construction should succeed."));
    expectEqual(window.testSessionCount(),
                0,
                QStringLiteral("The closed transaction-build session should be removed immediately."));
    expect(waitForCondition([&window]() {
               return window.testSessionCount() == 0;
           }, 500),
           QStringLiteral("Late Transaction build completion should not recreate or alter a closed session."));
}

void testMainWindowDiagnosticsIncludeTransactionWidgetState()
{
    CHIron::Gui::MainWindow window;
    window.resize(1000, 640);
    window.show();
    QApplication::processEvents();

    expect(window.testApplyTraceRows(buildTimelineTestRows(), QStringLiteral("transaction_diag.clogb")),
           QStringLiteral("Diagnostics transaction session should open."));
    window.testInjectTransactionSpans(2, 8);

    const QString snapshot = window.testDiagnosticsStateSnapshot();
    expect(snapshot.contains(QStringLiteral("View State: transaction=")),
           QStringLiteral("Diagnostics snapshot should include transaction widget view state."));
    expect(snapshot.contains(QStringLiteral("Transaction Widget: instanceId=")),
           QStringLiteral("Diagnostics snapshot should include a transaction widget instance id."));
    expect(snapshot.contains(QStringLiteral("status=Transaction spans ready: 8 spans across 2"))
               && snapshot.contains(QStringLiteral("buildGeneration="))
               && snapshot.contains(QStringLiteral("visibleStart="))
               && snapshot.contains(QStringLiteral("visibleEnd="))
               && snapshot.contains(QStringLiteral("stackRowCount=2")),
           QStringLiteral("Diagnostics snapshot should include useful Transaction widget runtime details."));
    expect(snapshot.contains(QStringLiteral("activeWidget=true"))
               && snapshot.contains(QStringLiteral("hasWidget=true")),
           QStringLiteral("Diagnostics snapshot should identify the active owning Transaction widget."));
}

void testMainWindowDiagnosticsIncludeCacheWidgetState()
{
    CHIron::Gui::MainWindow window;
    window.resize(1000, 640);
    window.show();
    QApplication::processEvents();

    expect(window.testApplyTraceRows(buildTimelineTestRows(), QStringLiteral("cache_diag.clogb")),
           QStringLiteral("Diagnostics cache session should open."));
    window.testInjectCacheSpans(2, 8);

    const QString snapshot = window.testDiagnosticsStateSnapshot();
    expect(snapshot.contains(QStringLiteral("View State: cache=")),
           QStringLiteral("Diagnostics snapshot should include Cache widget view state."));
    expect(snapshot.contains(QStringLiteral("Cache Widget: instanceId=")),
           QStringLiteral("Diagnostics snapshot should include a Cache widget instance id."));
    expect(snapshot.contains(QStringLiteral("status=Cache ready: 8 lifetimes across 2 RN bands."))
               && snapshot.contains(QStringLiteral("buildGeneration="))
               && snapshot.contains(QStringLiteral("visibleStart="))
               && snapshot.contains(QStringLiteral("visibleEnd="))
               && snapshot.contains(QStringLiteral("bandCount=2")),
           QStringLiteral("Diagnostics snapshot should include useful Cache widget runtime details."));
    expect(snapshot.contains(QStringLiteral("activeWidget=true"))
               && snapshot.contains(QStringLiteral("hasWidget=true")),
           QStringLiteral("Diagnostics snapshot should identify the active owning Cache widget."));
}

void testCoreRetryReissueAcceptedOnRNFAndSNF()
{
    using XactTestConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 256, true, true, true>;
    using ReqFlit = CHI::Eb::Flits::REQ<XactTestConfig>;
    using RspFlit = CHI::Eb::Flits::RSP<XactTestConfig>;

    CHI::Eb::Xact::Global<XactTestConfig> global;
    populateRetryTestTopology(global);

    const ReqFlit original = makeRetryReadRequest<ReqFlit>();
    const RspFlit retryAck = makeRetryAckResponse<RspFlit>();
    const RspFlit pcrdGrant = makePCrdGrantResponse<RspFlit>();
    const ReqFlit reissued = makeRetryReissuedRequest<ReqFlit>();
    ReqFlit snOriginalReq = original;
    snOriginalReq.SrcID() = 2;
    snOriginalReq.TgtID() = 3;
    ReqFlit snReissuedReq = snOriginalReq;
    snReissuedReq.AllowRetry() = 0;
    RspFlit snRetryAck = retryAck;
    snRetryAck.TgtID() = snOriginalReq.SrcID();
    snRetryAck.SrcID() = snOriginalReq.TgtID();
    RspFlit snPCrdGrant = pcrdGrant;
    snPCrdGrant.TgtID() = snOriginalReq.SrcID();
    snPCrdGrant.SrcID() = snOriginalReq.TgtID();

    CHI::Eb::Xact::RNFJoint<XactTestConfig> rnJoint;
    std::shared_ptr<CHI::Eb::Xact::Xaction<XactTestConfig>> rnOriginal;
    expect(rnJoint.NextTXREQ(global, 1, original, &rnOriginal)
               == CHI::Eb::Xact::XactDenial::ACCEPTED,
           QStringLiteral("RN-F should accept the original retryable request."));
    expect(rnJoint.NextRXRSP(global, 2, retryAck, &rnOriginal)
               == CHI::Eb::Xact::XactDenial::ACCEPTED,
           QStringLiteral("RN-F should accept RetryAck."));
    expect(rnJoint.NextRXRSP(global, 3, pcrdGrant)
               == CHI::Eb::Xact::XactDenial::ACCEPTED,
           QStringLiteral("RN-F should accept PCrdGrant."));
    std::shared_ptr<CHI::Eb::Xact::Xaction<XactTestConfig>> rnRetry;
    expect(rnJoint.NextTXREQ(global, 4, reissued, &rnRetry)
               == CHI::Eb::Xact::XactDenial::ACCEPTED,
           QStringLiteral("RN-F should accept the AllowRetry=0 reissued request."));
    expect(rnRetry && rnRetry->IsSecondTry() && rnRetry->GetFirstTry() == rnOriginal,
           QStringLiteral("RN-F reissued request should be linked to the original xaction."));

    CHI::Eb::Xact::SNFJoint<XactTestConfig> snJoint;
    std::shared_ptr<CHI::Eb::Xact::Xaction<XactTestConfig>> snOriginal;
    expect(snJoint.NextRXREQ(global, 1, snOriginalReq, &snOriginal)
               == CHI::Eb::Xact::XactDenial::ACCEPTED,
           QStringLiteral("SN-F should accept the original retryable request."));
    expect(snJoint.NextTXRSP(global, 2, snRetryAck, &snOriginal)
               == CHI::Eb::Xact::XactDenial::ACCEPTED,
           QStringLiteral("SN-F should accept RetryAck."));
    expect(snJoint.NextTXRSP(global, 3, snPCrdGrant)
               == CHI::Eb::Xact::XactDenial::ACCEPTED,
           QStringLiteral("SN-F should accept PCrdGrant."));
    std::shared_ptr<CHI::Eb::Xact::Xaction<XactTestConfig>> snRetry;
    expect(snJoint.NextRXREQ(global, 4, snReissuedReq, &snRetry)
               == CHI::Eb::Xact::XactDenial::ACCEPTED,
           QStringLiteral("SN-F should accept the AllowRetry=0 reissued request."));
    expect(snRetry && snRetry->IsSecondTry() && snRetry->GetFirstTry() == snOriginal,
           QStringLiteral("SN-F reissued request should be linked to the original xaction."));
}

void testXactionIndexRetryBoundaryReplaysPCrdGrant()
{
    const auto verifyBoundary = [](const RetryBoundarySplit split,
                                   const QString& fileName,
                                   const int expectedRows,
                                   const QString& context) {
        TransactionTraceFixture fixture = makeRetryChunkBoundaryTrace(fileName, split);

        CHIron::Gui::CLogBTraceLoadError error;
        std::shared_ptr<CHIron::Gui::TraceSession> session;
        expect(CHIron::Gui::TraceSession::open(fixture.tracePath, session, error),
               error.summary.isEmpty()
                   ? context + QStringLiteral(": retry boundary trace should open.")
                   : error.summary);
        expect(session != nullptr, context + QStringLiteral(": retry boundary session should be created."));
        expect(session->buildXactionIndex(error),
               error.summary.isEmpty()
                   ? context + QStringLiteral(": retry boundary trace should build an xaction index.")
                   : error.summary);

        std::vector<CHIron::Gui::TraceIssue> issues;
        expect(session->xactionIndexIssues(issues, error),
               error.summary.isEmpty()
                   ? context + QStringLiteral(": retry boundary issues should load.")
                   : error.summary);
        const auto retryIssue = std::find_if(issues.cbegin(), issues.cend(), [](const auto& issue) {
            return issue.denialName == QStringLiteral("XACT_DENIED_NO_MATCHING_RETRY")
                || issue.denialName == QStringLiteral("XACT_DENIED_NO_MATCHING_PCREDIT")
                || issue.denialName == QStringLiteral("XACT_DENIED_UNSUPPORTED_FEATURE");
        });
        expect(retryIssue == issues.cend(),
               context + QStringLiteral(": retry boundary replay should not persist retry/P-credit denials."));

        std::vector<CHIron::Gui::FlitRecord> rows;
        expect(session->readRows(0, expectedRows, rows, error),
               error.summary.isEmpty()
                   ? context + QStringLiteral(": retry boundary rows should read back.")
                   : error.summary);
        expectEqual(static_cast<int>(rows.size()),
                    expectedRows,
                    context + QStringLiteral(": retry boundary trace should read all rows."));
        expect(rows[0].xactionIndexed && rows[3].xactionIndexed,
               context + QStringLiteral(": original and reissued retry requests should both be indexed."));
        expect(!rows[0].transactionGroupKey.isEmpty()
                   && rows[0].transactionGroupKey == rows[3].transactionGroupKey,
               context + QStringLiteral(": original and reissued retry requests should share one persisted transaction key."));
    };

    verifyBoundary(RetryBoundarySplit::AfterRetryAck,
                   QStringLiteral("xaction_retry_after_retry_ack_boundary.clogb"),
                   4,
                   QStringLiteral("RetryAck boundary"));
    verifyBoundary(RetryBoundarySplit::AfterPCrdGrant,
                   QStringLiteral("xaction_retry_after_pcrd_grant_boundary.clogb"),
                   6,
                   QStringLiteral("PCrdGrant boundary"));
}

void testMainWindowTransactionRefreshFollowsOwningXactionSession()
{
    using XactTestConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 256, true, true, true>;
    using ReqFlit = CHI::Eb::Flits::REQ<XactTestConfig>;
    using DatFlit = CHI::Eb::Flits::DAT<XactTestConfig>;

    CLog::Parameters params;
    params.SetIssue(CLog::Issue::Eb);
    expect(params.SetNodeIdWidth(11), QStringLiteral("Failed to set test NodeIDWidth."));
    expect(params.SetReqAddrWidth(52), QStringLiteral("Failed to set test ReqAddrWidth."));
    expect(params.SetReqRSVDCWidth(32), QStringLiteral("Failed to set test ReqRSVDCWidth."));
    expect(params.SetDatRSVDCWidth(32), QStringLiteral("Failed to set test DatRSVDCWidth."));
    expect(params.SetDataWidth(256), QStringLiteral("Failed to set test DataWidth."));
    params.SetDataCheckPresent(true);
    params.SetPoisonPresent(true);
    params.SetMPAMPresent(true);

    ReqFlit firstReq{};
    firstReq.TgtID() = 2;
    firstReq.SrcID() = 1;
    firstReq.TxnID() = 7;
    firstReq.Opcode() = CHI::Eb::Opcodes::REQ::ReadNoSnp;
    firstReq.Size() = CHI::Eb::Sizes::B64;
    firstReq.Addr() = 0x1000;
    firstReq.AllowRetry() = 1;

    DatFlit firstData0{};
    firstData0.TgtID() = 1;
    firstData0.SrcID() = 2;
    firstData0.TxnID() = 7;
    firstData0.HomeNID() = 2;
    firstData0.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
    firstData0.RespErr() = CHI::Eb::RespErrs::OK;
    firstData0.Resp() = CHI::Eb::Resps::CompData_UC;
    firstData0.DBID() = 9;
    firstData0.DataID() = 0;
    firstData0.BE() = 0xFFFFFFFFU;
    DatFlit firstData2 = firstData0;
    firstData2.DataID() = 2;

    ReqFlit secondReq = firstReq;
    secondReq.TxnID() = 11;
    secondReq.Addr() = 0x2000;
    DatFlit secondData0 = firstData0;
    secondData0.TxnID() = 11;
    secondData0.DBID() = 14;
    DatFlit secondData2 = secondData0;
    secondData2.DataID() = 2;

    const auto serializeReq = [](const ReqFlit& flit, TestFlitBitWriter& writer) {
        appendEbReqFlitForTest(flit, writer);
    };
    const auto serializeDat = [](const DatFlit& flit, TestFlitBitWriter& writer) {
        appendEbDatFlitForTest(flit, writer);
    };

    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    const QString firstTracePath = tempDir.filePath(QStringLiteral("transaction_refresh_a.clogb"));
    const QString secondTracePath = tempDir.filePath(QStringLiteral("transaction_refresh_b.clogb"));
    writeTraceWithTopology(firstTracePath,
                           params,
                           {
                               {CLog::NodeType::RN_F, 1},
                               {CLog::NodeType::HN_F, 2},
                           },
                           {
                               makeSerializedRecord(CLog::Channel::TXREQ, 1, 2, firstReq, serializeReq),
                               makeSerializedRecord(CLog::Channel::RXDAT, 1, 3, firstData0, serializeDat),
                               makeSerializedRecord(CLog::Channel::RXDAT, 1, 5, firstData2, serializeDat),
                           });
    writeTraceWithTopology(secondTracePath,
                           params,
                           {
                               {CLog::NodeType::RN_F, 1},
                               {CLog::NodeType::HN_F, 2},
                           },
                           {
                               makeSerializedRecord(CLog::Channel::TXREQ, 1, 2, secondReq, serializeReq),
                               makeSerializedRecord(CLog::Channel::RXDAT, 1, 3, secondData0, serializeDat),
                               makeSerializedRecord(CLog::Channel::RXDAT, 1, 5, secondData2, serializeDat),
                           });

    CHIron::Gui::CLogBTraceLoadError prebuildError;
    std::shared_ptr<CHIron::Gui::TraceSession> prebuiltFirstSession;
    expect(CHIron::Gui::TraceSession::open(firstTracePath, prebuiltFirstSession, prebuildError),
           prebuildError.summary.isEmpty()
               ? QStringLiteral("First transaction refresh trace should open for prebuilt xaction index.")
               : prebuildError.summary);
    expect(prebuiltFirstSession != nullptr,
           QStringLiteral("First prebuilt transaction refresh session should be created."));
    expect(prebuiltFirstSession->buildXactionIndex(prebuildError),
           prebuildError.summary.isEmpty()
               ? QStringLiteral("First transaction refresh trace should get an existing xaction index before reload.")
               : prebuildError.summary);
    prebuiltFirstSession.reset();

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(firstTracePath),
           QStringLiteral("First transaction refresh trace should load."));
    window.testBuildVisualizationViews();
    const int firstWidgetId = window.testTransactionWidgetInstanceId();
    expect(waitForCondition([&window]() {
               return window.testTransactionSpanCount() == 1;
           }, 5000),
           QStringLiteral("First pre-indexed session should build initial Transaction spans."));
    window.testStartXactionIndexing(true);

    expect(window.testLoadTraceFile(secondTracePath),
           QStringLiteral("Second transaction refresh trace should load while first indexing may still complete."));
    const int secondWidgetId = window.testTransactionWidgetInstanceId();
    expect(secondWidgetId != firstWidgetId,
           QStringLiteral("Second trace should have a separate Transaction widget before indexing."));
    expectEqual(window.testTransactionSpanCount(),
                0,
                QStringLiteral("Active second session should not receive first session transaction spans."));

    expect(waitForCondition([&window]() {
               return !window.testSessionXactionIndexActive(0);
           }, 5000),
           QStringLiteral("First session xaction index rebuild should finish in the background."));
    QApplication::processEvents();
    expectEqual(window.testActiveSessionIndex(),
                1,
                QStringLiteral("First session xaction completion should not switch the active session."));
    expectEqual(window.testTransactionWidgetInstanceId(),
                secondWidgetId,
                QStringLiteral("First session xaction completion should not replace active Transaction widget."));
    expectEqual(window.testTransactionSpanCount(),
                0,
                QStringLiteral("Inactive session xaction completion should not populate active Transaction spans."));

    expect(window.testSwitchToSession(0),
           QStringLiteral("Switching to first indexed session should succeed."));
    window.testBuildVisualizationViews();
    expect(waitForCondition([&window]() {
               return window.testTransactionSpanCount() == 1;
           }, 5000),
           QStringLiteral("First session Transaction widget should build spans after its owning xaction index rebuild completes."));
    expectEqual(window.testTransactionWidgetInstanceId(),
                firstWidgetId,
                QStringLiteral("First indexed session should reuse its original Transaction widget."));

    expect(window.testSwitchToSession(1),
           QStringLiteral("Switching back to unindexed second session should succeed."));
    QApplication::processEvents();
    expectEqual(window.testTransactionWidgetInstanceId(),
                secondWidgetId,
                QStringLiteral("Second unindexed session should reuse its original Transaction widget."));
    expectEqual(window.testTransactionSpanCount(),
                0,
                QStringLiteral("Second unindexed session should remain without Transaction spans."));
}

void testMainWindowTransactionSessionSwitchesDoNotRebuild()
{
    TransactionTraceFixture firstFixture =
        makeIndexedReadNoSnpTrace(QStringLiteral("transaction_no_rebuild_a.clogb"), 12);
    TransactionTraceFixture secondFixture =
        makeIndexedReadNoSnpTrace(QStringLiteral("transaction_no_rebuild_b.clogb"), 18);

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(firstFixture.tracePath),
           QStringLiteral("First no-rebuild transaction fixture should load."));
    window.testBuildVisualizationViews();
    expect(waitForCondition([&window, expected = firstFixture.transactionCount]() {
               QApplication::processEvents();
               return window.testTransactionSpanCount() == expected;
           }, 5000),
           QStringLiteral("First no-rebuild transaction session should build spans."));
    const int firstWidgetId = window.testTransactionWidgetInstanceId();
    const quint64 firstGeneration = window.testTransactionBuildGeneration();
    const QString firstRangeText = window.testTransactionVisibleRangeText();

    expect(window.testLoadTraceFile(secondFixture.tracePath),
           QStringLiteral("Second no-rebuild transaction fixture should load."));
    window.testBuildVisualizationViews();
    expect(waitForCondition([&window, expected = secondFixture.transactionCount]() {
               QApplication::processEvents();
               return window.testTransactionSpanCount() == expected;
           }, 5000),
           QStringLiteral("Second no-rebuild transaction session should build spans."));
    const int secondWidgetId = window.testTransactionWidgetInstanceId();
    const quint64 secondGeneration = window.testTransactionBuildGeneration();
    const QString secondRangeText = window.testTransactionVisibleRangeText();

    for (int iteration = 0; iteration < 4; ++iteration) {
        expect(window.testSwitchToSession(0),
               QStringLiteral("Switching to first no-rebuild transaction session should succeed."));
        QApplication::processEvents();
        expectEqual(window.testTransactionWidgetInstanceId(),
                    firstWidgetId,
                    QStringLiteral("First no-rebuild session should reuse its Transaction widget."));
        expectEqual(static_cast<int>(window.testTransactionBuildGeneration()),
                    static_cast<int>(firstGeneration),
                    QStringLiteral("Switching to first session should not restart Transaction span building."));
        expectEqual(window.testTransactionVisibleRangeText(),
                    firstRangeText,
                    QStringLiteral("First no-rebuild session should keep its Transaction visible range."));

        expect(window.testSwitchToSession(1),
               QStringLiteral("Switching to second no-rebuild transaction session should succeed."));
        QApplication::processEvents();
        expectEqual(window.testTransactionWidgetInstanceId(),
                    secondWidgetId,
                    QStringLiteral("Second no-rebuild session should reuse its Transaction widget."));
        expectEqual(static_cast<int>(window.testTransactionBuildGeneration()),
                    static_cast<int>(secondGeneration),
                    QStringLiteral("Switching to second session should not restart Transaction span building."));
        expectEqual(window.testTransactionVisibleRangeText(),
                    secondRangeText,
                    QStringLiteral("Second no-rebuild session should keep its Transaction visible range."));
    }
}

void testMainWindowPreservesPerSessionTopologySelection()
{
    const CLog::Parameters parameters = makeDefaultTestParameters();
    const std::uint8_t reqBytes = storedByteLengthForTest(CHIron::Gui::ReqFlit::WIDTH);

    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    const QString firstTracePath = tempDir.filePath(QStringLiteral("topology_state_a.clogb"));
    const QString secondTracePath = tempDir.filePath(QStringLiteral("topology_state_b.clogb"));
    writeTraceWithTopology(firstTracePath,
                           parameters,
                           {
                               {CLog::NodeType::RN_F, 1},
                               {CLog::NodeType::HN_F, 2},
                           },
                           {makeLengthOnlyRecord(CLog::Channel::TXREQ, reqBytes)});
    writeTraceWithTopology(secondTracePath,
                           parameters,
                           {
                               {CLog::NodeType::RN_F, 3},
                               {CLog::NodeType::SN_F, 4},
                           },
                           {makeLengthOnlyRecord(CLog::Channel::TXREQ, reqBytes)});

    CHIron::Gui::MainWindow window;
    window.resize(1000, 640);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(firstTracePath),
           QStringLiteral("First topology-state trace should load."));
    window.testSelectTopologyNode(2);
    expectEqual(window.testSelectedTopologyNode(),
                2,
                QStringLiteral("First topology selection should be visible before switching."));

    expect(window.testLoadTraceFile(secondTracePath),
           QStringLiteral("Second topology-state trace should load."));
    window.testSelectTopologyNode(4);
    expectEqual(window.testSelectedTopologyNode(),
                4,
                QStringLiteral("Second topology selection should be visible before switching."));

    expect(window.testSwitchToSession(0),
           QStringLiteral("Switching back to first topology session should succeed."));
    QApplication::processEvents();
    expectEqual(window.testSelectedTopologyNode(),
                2,
                QStringLiteral("First session topology selection should be restored."));

    expect(window.testSwitchToSession(1),
           QStringLiteral("Switching back to second topology session should succeed."));
    QApplication::processEvents();
    expectEqual(window.testSelectedTopologyNode(),
                4,
                QStringLiteral("Second session topology selection should be restored."));
}

void testMainWindowSessionCloseActions()
{
    CHIron::Gui::MainWindow window;
    window.resize(1000, 640);
    window.show();
    QApplication::processEvents();

    expect(window.testApplyTraceRows(buildFilterTestRows(), QStringLiteral("close_a.clogb")),
           QStringLiteral("First close-action session should open."));
    expect(window.testApplyTraceRows(buildFilterTestRows(), QStringLiteral("close_b.clogb")),
           QStringLiteral("Second close-action session should open."));
    expect(window.testApplyTraceRows(buildFilterTestRows(), QStringLiteral("close_c.clogb")),
           QStringLiteral("Third close-action session should open."));
    expectEqual(window.testSessionCount(),
                3,
                QStringLiteral("Close-action setup should create three sessions."));

    expect(window.testSwitchToSession(1),
           QStringLiteral("Switching to the middle session before close-current should succeed."));
    expect(window.testCloseActiveSession(),
           QStringLiteral("Closing the active middle session should succeed."));
    expectEqual(window.testSessionCount(),
                2,
                QStringLiteral("Closing the active session should leave the other sessions open."));
    expectEqual(window.testActiveSessionIndex(),
                1,
                QStringLiteral("Closing an active middle session should select the nearest remaining session."));
    expectEqual(window.testActiveSessionLabel(),
                QStringLiteral("close_c.clogb"),
                QStringLiteral("Closing an active middle session should activate the right neighbor."));

    expect(window.testCloseOtherSessions(0),
           QStringLiteral("Closing other sessions should succeed."));
    expectEqual(window.testSessionCount(),
                1,
                QStringLiteral("Close other sessions should keep only the requested session."));

    window.testCloseAllSessions();
    expectEqual(window.testSessionCount(),
                0,
                QStringLiteral("Close all sessions should return the main window to the empty state."));
}

void testMainWindowReloadsOnlyActiveSession()
{
    const CLog::Parameters parameters = makeDefaultTestParameters();
    const std::uint8_t reqBytes = storedByteLengthForTest(CHIron::Gui::ReqFlit::WIDTH);
    const std::uint8_t rspBytes = storedByteLengthForTest(CHIron::Gui::RspFlit::WIDTH);
    const std::uint8_t datBytes = storedByteLengthForTest(CHIron::Gui::DatFlit::WIDTH);

    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    const QString firstTracePath = tempDir.filePath(QStringLiteral("reload_a.clogb"));
    const QString secondTracePath = tempDir.filePath(QStringLiteral("reload_b.clogb"));
    writeLengthOnlyTrace(firstTracePath,
                         parameters,
                         {makeLengthOnlyRecord(CLog::Channel::TXREQ, reqBytes)});
    writeLengthOnlyTrace(secondTracePath,
                         parameters,
                         {makeLengthOnlyRecord(CLog::Channel::TXRSP, rspBytes)});

    CHIron::Gui::MainWindow window;
    window.resize(1000, 640);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(firstTracePath),
           QStringLiteral("First reload test trace should open."));
    expect(window.testLoadTraceFile(secondTracePath),
           QStringLiteral("Second reload test trace should open."));
    expectEqual(window.testSessionCount(),
                2,
                QStringLiteral("Reload test should start with two sessions."));
    expect(window.testSwitchToSession(0),
           QStringLiteral("Switching to the first session before reload should succeed."));
    const QStringList labelsBeforeReload = window.testSessionLabels();

    writeLengthOnlyTrace(firstTracePath,
                         parameters,
                         {
                             makeLengthOnlyRecord(CLog::Channel::TXREQ, reqBytes),
                             makeLengthOnlyRecord(CLog::Channel::TXDAT, datBytes),
                         });
    expect(window.testReloadActiveTrace(),
           QStringLiteral("Reloading the active file-backed session should succeed."));
    QApplication::processEvents();
    expectEqual(window.testSessionCount(),
                2,
                QStringLiteral("Reloading should replace the active session in-place instead of opening a third session."));
    expectEqual(window.testActiveSessionIndex(),
                0,
                QStringLiteral("Reloading the active session should preserve its tab position."));
    expectEqual(window.testSessionLabels().join(QLatin1Char('|')),
                labelsBeforeReload.join(QLatin1Char('|')),
                QStringLiteral("Reloading the same file should preserve session labels."));
    expectEqual(window.testVisibleTraceRowCount(),
                2,
                QStringLiteral("Active session should show the reloaded row count."));

    expect(window.testSwitchToSession(1),
           QStringLiteral("Switching to the second session after reload should succeed."));
    expectEqual(window.testVisibleTraceRowCount(),
                1,
                QStringLiteral("Reloading session A should not modify session B."));
    expectEqual(window.testActiveSessionLabel(),
                QStringLiteral("reload_b.clogb"),
                QStringLiteral("Session B should keep its label after session A reloads."));
}

void testMainWindowReloadFailureKeepsOldSessionUsable()
{
    const CLog::Parameters parameters = makeDefaultTestParameters();
    const std::uint8_t reqBytes = storedByteLengthForTest(CHIron::Gui::ReqFlit::WIDTH);
    const std::uint8_t rspBytes = storedByteLengthForTest(CHIron::Gui::RspFlit::WIDTH);

    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    const QString firstTracePath = tempDir.filePath(QStringLiteral("reload_failure_a.clogb"));
    const QString secondTracePath = tempDir.filePath(QStringLiteral("reload_failure_b.clogb"));
    writeLengthOnlyTrace(firstTracePath,
                         parameters,
                         {makeLengthOnlyRecord(CLog::Channel::TXREQ, reqBytes)});
    writeLengthOnlyTrace(secondTracePath,
                         parameters,
                         {makeLengthOnlyRecord(CLog::Channel::TXRSP, rspBytes)});

    CHIron::Gui::MainWindow window;
    window.resize(1000, 640);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(firstTracePath),
           QStringLiteral("First reload-failure trace should open."));
    expect(window.testLoadTraceFile(secondTracePath),
           QStringLiteral("Second reload-failure trace should open."));
    expect(window.testSwitchToSession(0),
           QStringLiteral("Switching to the first session before failed reload should succeed."));
    const QStringList labelsBeforeFailure = window.testSessionLabels();
    const int activeBeforeFailure = window.testActiveSessionIndex();
    const int rowCountBeforeFailure = window.testVisibleTraceRowCount();

    expect(QFile::remove(firstTracePath),
           QStringLiteral("Removing the active trace file should set up a reload failure."));
    expect(!window.testReloadActiveTrace(),
           QStringLiteral("Reloading a missing active trace should fail."));
    QApplication::processEvents();
    expectEqual(window.testSessionCount(),
                2,
                QStringLiteral("A failed reload should keep all open sessions alive."));
    expectEqual(window.testActiveSessionIndex(),
                activeBeforeFailure,
                QStringLiteral("A failed reload should keep the same active session."));
    expectEqual(window.testSessionLabels().join(QLatin1Char('|')),
                labelsBeforeFailure.join(QLatin1Char('|')),
                QStringLiteral("A failed reload should not rename or reorder sessions."));
    expectEqual(window.testVisibleTraceRowCount(),
                rowCountBeforeFailure,
                QStringLiteral("A failed reload should keep the old active trace data usable."));

    expect(window.testSwitchToSession(1),
           QStringLiteral("The other session should remain selectable after a failed reload."));
    expectEqual(window.testVisibleTraceRowCount(),
                1,
                QStringLiteral("The other session should remain readable after a failed reload."));
}

void testMainWindowDiagnosticsSnapshotSummarizesMultipleSessions()
{
    const CLog::Parameters parameters = makeDefaultTestParameters();
    const std::uint8_t reqBytes = storedByteLengthForTest(CHIron::Gui::ReqFlit::WIDTH);
    const std::uint8_t rspBytes = storedByteLengthForTest(CHIron::Gui::RspFlit::WIDTH);

    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    const QString firstTracePath = tempDir.filePath(QStringLiteral("diagnostics_a.clogb"));
    const QString secondTracePath = tempDir.filePath(QStringLiteral("diagnostics_b.clogb"));
    writeLengthOnlyTrace(firstTracePath,
                         parameters,
                         {
                             makeLengthOnlyRecord(CLog::Channel::TXREQ, reqBytes),
                             makeLengthOnlyRecord(CLog::Channel::TXREQ, reqBytes),
                         });
    writeLengthOnlyTrace(secondTracePath,
                         parameters,
                         {makeLengthOnlyRecord(CLog::Channel::TXRSP, rspBytes)});

    CHIron::Gui::MainWindow window;
    window.resize(1000, 640);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(firstTracePath),
           QStringLiteral("First diagnostics trace should open."));
    expect(window.testLoadTraceFile(secondTracePath),
           QStringLiteral("Second diagnostics trace should open."));
    expect(window.testSwitchToSession(0),
           QStringLiteral("Switching to the first diagnostics session should succeed."));
    window.testStartStatisticsComputation();
    expect(waitForCondition([&window]() {
               return window.testSessionStatisticsComputed(0);
           },
           15000),
           QStringLiteral("Diagnostics statistics scan should finish for the first session."));
    expect(window.testSwitchToSession(1),
           QStringLiteral("Switching to the second diagnostics session should succeed."));
    QVariantMap latencyState;
    latencyState.insert(QStringLiteral("plotType"), QStringLiteral("Violin"));
    latencyState.insert(QStringLiteral("plotHasCursor"), true);
    latencyState.insert(QStringLiteral("plotCursorValue"), 42.0);
    window.testRestoreLatencyViewState(latencyState);

    const QString snapshot = window.testDiagnosticsStateSnapshot();
    const auto contains = [&snapshot](const QString& text) {
        return snapshot.contains(text);
    };

    expect(contains(QStringLiteral("Open Session Count: 2")),
           QStringLiteral("Diagnostics snapshot should include the number of open sessions."));
    expect(contains(QStringLiteral("Open Trace Sessions")),
           QStringLiteral("Diagnostics snapshot should include the multi-session section."));
    expect(contains(QStringLiteral("Session[0]: id=")),
           QStringLiteral("Diagnostics snapshot should include the first session id."));
    expect(contains(QStringLiteral("Session[1]: id=")),
           QStringLiteral("Diagnostics snapshot should include the second session id."));
    expect(contains(QStringLiteral("label=\"diagnostics_a.clogb\"")),
           QStringLiteral("Diagnostics snapshot should include the first session label."));
    expect(contains(QStringLiteral("label=\"diagnostics_b.clogb\"")),
           QStringLiteral("Diagnostics snapshot should include the second session label."));
    expect(contains(QStringLiteral("path=\"") + QDir::toNativeSeparators(firstTracePath) + QStringLiteral("\"")),
           QStringLiteral("Diagnostics snapshot should include the first session path."));
    expect(contains(QStringLiteral("Rows: total=2")),
           QStringLiteral("Diagnostics snapshot should include per-session row counts."));
    expect(contains(QStringLiteral("statisticsComputed=true")),
           QStringLiteral("Diagnostics snapshot should include per-session statistics state."));
    expect(contains(QStringLiteral("xactionIndexActive=false")),
           QStringLiteral("Diagnostics snapshot should include per-session xaction-index state."));
    expect(contains(QStringLiteral("View State: timeline=")),
           QStringLiteral("Diagnostics snapshot should include timeline view state."));
    expect(contains(QStringLiteral("View State: address=")),
           QStringLiteral("Diagnostics snapshot should include address view state."));
    expect(contains(QStringLiteral("View State: latency="))
               && contains(QStringLiteral("plotType=Violin")),
           QStringLiteral("Diagnostics snapshot should include active latency view state."));
    expect(contains(QStringLiteral(".fastidx"))
               && contains(QStringLiteral(".xactidx")),
           QStringLiteral("Diagnostics snapshot should include sidecar index paths keyed by trace path."));
}

void testMainWindowSessionBackedModelsStayLazyAndCachesBounded()
{
    const CLog::Parameters parameters = makeDefaultTestParameters();
    const std::uint8_t reqBytes = storedByteLengthForTest(CHIron::Gui::ReqFlit::WIDTH);
    const std::uint8_t rspBytes = storedByteLengthForTest(CHIron::Gui::RspFlit::WIDTH);
    const std::uint8_t datBytes = storedByteLengthForTest(CHIron::Gui::DatFlit::WIDTH);
    const std::uint8_t snpBytes = storedByteLengthForTest(CHIron::Gui::SnpFlit::WIDTH);

    constexpr int kBlocksPerSession = 32;
    constexpr int kRowsPerBlock = 1024;
    constexpr int kRowsPerSession = kBlocksPerSession * kRowsPerBlock;
    bool sessionCountFromEnvironment = false;
    const int requestedSessionCount =
        qEnvironmentVariableIntValue("CHIRON_GUI_STAGE8_SESSION_COUNT", &sessionCountFromEnvironment);
    const int sessionCount = sessionCountFromEnvironment
        ? std::clamp(requestedSessionCount, 1, 4)
        : 4;

    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));

    QStringList tracePaths;
    tracePaths.reserve(sessionCount);
    qint64 totalTraceBytes = 0;
    for (int sessionIndex = 0; sessionIndex < sessionCount; ++sessionIndex) {
        std::vector<std::vector<CLog::CLogB::TagCHIRecords::Record>> blocks;
        blocks.reserve(kBlocksPerSession);
        for (int blockIndex = 0; blockIndex < kBlocksPerSession; ++blockIndex) {
            std::vector<CLog::CLogB::TagCHIRecords::Record> records;
            records.reserve(kRowsPerBlock);
            for (int row = 0; row < kRowsPerBlock; ++row) {
                const int channelSelector = (sessionIndex + blockIndex + row) % 4;
                records.push_back(channelSelector == 0 ? makeLengthOnlyRecord(CLog::Channel::TXREQ, reqBytes)
                                  : channelSelector == 1 ? makeLengthOnlyRecord(CLog::Channel::TXRSP, rspBytes)
                                  : channelSelector == 2 ? makeLengthOnlyRecord(CLog::Channel::TXDAT, datBytes)
                                                         : makeLengthOnlyRecord(CLog::Channel::TXSNP, snpBytes));
            }
            blocks.push_back(std::move(records));
        }

        const QString tracePath = tempDir.filePath(QStringLiteral("stage8_lazy_cache_%1.clogb").arg(sessionIndex));
        writeLengthOnlyTraceBlocks(tracePath, parameters, std::move(blocks));
        const QFileInfo traceInfo(tracePath);
        expect(traceInfo.exists() && traceInfo.size() > 0,
               QStringLiteral("Synthetic Stage 8 trace should be written before loading."));
        totalTraceBytes += traceInfo.size();
        tracePaths.append(tracePath);
    }

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();

    for (const QString& tracePath : tracePaths) {
        expect(window.testLoadTraceFile(tracePath),
               QStringLiteral("Stage 8 synthetic trace should open as a file-backed session."));
        expect(window.testActiveModelIsSessionBacked(),
               QStringLiteral("File-backed trace sessions should attach the table to TraceSession."));
        expectEqual(window.testActiveModelSourceRowsSnapshotSize(),
                    0,
                    QStringLiteral("Session-backed table models should not duplicate decoded source rows."));
    }

    expectEqual(window.testSessionCount(),
                sessionCount,
                QStringLiteral("Stage 8 synthetic benchmark should open all sessions."));
    expect(totalTraceBytes > 0,
           QStringLiteral("Stage 8 synthetic benchmark should account for generated file sizes."));

    QElapsedTimer switchTimer;
    switchTimer.start();
    for (int iteration = 0; iteration < 3; ++iteration) {
        for (int sessionIndex = 0; sessionIndex < sessionCount; ++sessionIndex) {
            expect(window.testSwitchToSession(sessionIndex),
                   QStringLiteral("Switching synthetic Stage 8 sessions should succeed."));
            QApplication::processEvents();
            expect(window.testActiveModelIsSessionBacked(),
                   QStringLiteral("Switching should keep the active model session-backed."));
            expectEqual(window.testActiveModelSourceRowsSnapshotSize(),
                        0,
                        QStringLiteral("Switching should not materialize a full decoded-row snapshot."));

            window.testSelectLogicalRow(0);
            window.testSelectLogicalRow(kRowsPerSession / 2);
            window.testSelectLogicalRow(kRowsPerSession - 1);
            QApplication::processEvents();

            expect(window.testActiveTraceSessionCachedPageCount()
                       <= window.testActiveTraceSessionMaxCachedPages(),
                   QStringLiteral("TraceSession page cache should remain within its configured cap."));
            expect(window.testActiveTraceSessionCachedBlockCount()
                       <= window.testActiveTraceSessionMaxCachedBlocks(),
                   QStringLiteral("TraceSession decoded-block cache should remain within its configured cap."));
        }
    }
    const qint64 switchElapsedMs = switchTimer.elapsed();

    expect(switchElapsedMs < 30000,
           QStringLiteral("Stage 8 synthetic switching should stay responsive enough for the critical suite."));

    const QString snapshot = window.testDiagnosticsStateSnapshot();
    expect(snapshot.contains(QStringLiteral("Memory: working="))
               && snapshot.contains(QStringLiteral("Open Session Count: %1").arg(sessionCount))
               && snapshot.contains(QStringLiteral("Page Cache: size="))
               && snapshot.contains(QStringLiteral("Block Cache: cached=")),
           QStringLiteral("Diagnostics should expose memory and active cache counters for benchmark review."));

    if (qEnvironmentVariableIsSet("CHIRON_GUI_STAGE8_PRINT_METRICS")) {
        QString memoryLine;
        for (const QString& line : snapshot.split(QLatin1Char('\n'))) {
            if (line.startsWith(QStringLiteral("Memory: "))) {
                memoryLine = line;
                break;
            }
        }
        std::printf("Stage8Metrics sessions=%d rows_per_session=%d total_rows=%d total_trace_bytes=%lld switch_ms=%lld memory=\"%s\"\n",
                    sessionCount,
                    kRowsPerSession,
                    sessionCount * kRowsPerSession,
                    static_cast<long long>(totalTraceBytes),
                    static_cast<long long>(switchElapsedMs),
                    memoryLine.toUtf8().constData());
        std::fflush(stdout);
    }
}

void testMainWindowLoadFailureDoesNotCorruptExistingSessions()
{
    CLog::Parameters parameters;
    parameters.SetIssue(CLog::Issue::Eb);
    expect(parameters.SetNodeIdWidth(CHIron::Gui::ViewerConfig::nodeIdWidth),
           QStringLiteral("Failed to set test NodeIDWidth."));
    expect(parameters.SetReqAddrWidth(CHIron::Gui::ViewerConfig::reqAddrWidth),
           QStringLiteral("Failed to set test ReqAddrWidth."));
    expect(parameters.SetReqRSVDCWidth(CHIron::Gui::ViewerConfig::reqRsvdcWidth),
           QStringLiteral("Failed to set test ReqRSVDCWidth."));
    expect(parameters.SetDatRSVDCWidth(CHIron::Gui::ViewerConfig::datRsvdcWidth),
           QStringLiteral("Failed to set test DatRSVDCWidth."));
    expect(parameters.SetDataWidth(CHIron::Gui::ViewerConfig::dataWidth),
           QStringLiteral("Failed to set test DataWidth."));
    parameters.SetDataCheckPresent(CHIron::Gui::ViewerConfig::dataCheckWidth != 0);
    parameters.SetPoisonPresent(CHIron::Gui::ViewerConfig::poisonWidth != 0);
    parameters.SetMPAMPresent(CHIron::Gui::ViewerConfig::mpamWidth != 0);

    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    const QString firstTracePath = tempDir.filePath(QStringLiteral("first_load.clogb"));
    const QString secondTracePath = tempDir.filePath(QStringLiteral("second_load.clogb"));
    const std::uint8_t reqBytes = storedByteLengthForTest(CHIron::Gui::ReqFlit::WIDTH);
    writeLengthOnlyTrace(firstTracePath,
                         parameters,
                         {makeLengthOnlyRecord(CLog::Channel::TXREQ, reqBytes)});
    writeLengthOnlyTrace(secondTracePath,
                         parameters,
                         {makeLengthOnlyRecord(CLog::Channel::TXREQ, reqBytes)});

    CHIron::Gui::MainWindow window;
    window.resize(1000, 640);
    window.show();
    QApplication::processEvents();

    expect(window.testLoadTraceFile(firstTracePath),
           QStringLiteral("The first direct load-path trace should open."));
    expect(window.testLoadTraceFile(secondTracePath),
           QStringLiteral("The second direct load-path trace should open as another session."));
    expectEqual(window.testSessionCount(),
                2,
                QStringLiteral("Direct load path should support loading two trace sessions."));

    const QStringList labelsBeforeFailure = window.testSessionLabels();
    const int activeBeforeFailure = window.testActiveSessionIndex();
    const QString missingPath = tempDir.filePath(QStringLiteral("missing.clogb"));
    expect(!window.testLoadTraceFile(missingPath),
           QStringLiteral("Loading a missing trace through the direct path should fail."));
    expectEqual(window.testSessionCount(),
                2,
                QStringLiteral("A failed load should not close existing sessions."));
    expectEqual(window.testActiveSessionIndex(),
                activeBeforeFailure,
                QStringLiteral("A failed load should not change the active session."));
    expectEqual(window.testSessionLabels().join(QLatin1Char('|')),
                labelsBeforeFailure.join(QLatin1Char('|')),
                QStringLiteral("A failed load should not rename or reorder existing sessions."));
}

void testMainWindowStatisticsCompletionStaysWithOriginSession()
{
    CHIron::Gui::MainWindow window;
    window.resize(1000, 640);
    window.show();
    QApplication::processEvents();

    expect(window.testApplyTraceRows(buildStatisticsStressRows(180000), QStringLiteral("stats_a.clogb")),
           QStringLiteral("Statistics origin session should open."));
    expect(window.testApplyTraceRows(buildFilterTestRows(), QStringLiteral("stats_b.clogb")),
           QStringLiteral("Second session should open for statistics switch isolation."));
    expect(window.testSwitchToSession(0),
           QStringLiteral("Switching to statistics origin session should succeed."));
    window.testStartStatisticsComputation();
    expect(window.testSessionStatisticsActive(0),
           QStringLiteral("Statistics scan should start on session A."));

    expect(window.testSwitchToSession(1),
           QStringLiteral("Switching away during statistics scan should succeed."));
    expectEqual(window.testActiveSessionIndex(),
                1,
                QStringLiteral("Session B should remain active while session A statistics complete."));

    expect(waitForCondition([&window]() {
               return window.testSessionStatisticsComputed(0);
           },
           15000),
           QStringLiteral("Statistics scan should finish on its origin session after switching away."));
    expectEqual(window.testActiveSessionIndex(),
                1,
                QStringLiteral("Statistics completion from session A should not switch the active session."));
    expect(!window.testActiveSessionStatisticsComputed(),
           QStringLiteral("Statistics completion from session A should not mark active session B as computed."));
}

void testMainWindowCloseSessionWithActiveStatisticsIgnoresLateChunks()
{
    CHIron::Gui::MainWindow window;
    window.resize(1000, 640);
    window.show();
    QApplication::processEvents();

    expect(window.testApplyTraceRows(buildStatisticsStressRows(180000), QStringLiteral("closing_stats_a.clogb")),
           QStringLiteral("Statistics close origin session should open."));
    expect(window.testApplyTraceRows(buildFilterTestRows(), QStringLiteral("closing_stats_b.clogb")),
           QStringLiteral("Second session should open before closing active statistics session."));
    expect(window.testSwitchToSession(0),
           QStringLiteral("Switching to active statistics session should succeed."));
    window.testStartStatisticsComputation();
    expect(window.testSessionStatisticsActive(0),
           QStringLiteral("Statistics scan should be active before closing its session."));

    expect(window.testCloseSession(0),
           QStringLiteral("Closing a session with active statistics should succeed."));
    QApplication::processEvents();
    expectEqual(window.testSessionCount(),
                1,
                QStringLiteral("Closing active statistics session should leave the other session open."));
    expectEqual(window.testActiveSessionLabel(),
                QStringLiteral("closing_stats_b.clogb"),
                QStringLiteral("Remaining session should become active after closing the statistics origin."));
    expect(waitForCondition([&window]() {
               return window.testSessionCount() == 1
                   && window.testActiveSessionLabel() == QStringLiteral("closing_stats_b.clogb");
           },
           300),
           QStringLiteral("Late statistics chunks should not corrupt the remaining session."));
    expect(!window.testActiveSessionStatisticsComputed(),
           QStringLiteral("Late statistics chunks from a closed session should not update the remaining session."));
}

void testDeniedFlitFilterShowsOnlyDeniedXactionRows()
{
    std::vector<CHIron::Gui::FlitRecord> rows = buildFilterTestRows();
    rows[0].xactionIndexState = CHIron::Gui::XactionIndexState::Indexed;
    rows[1].xactionIndexState = CHIron::Gui::XactionIndexState::Denied;
    rows[2].xactionIndexState = CHIron::Gui::XactionIndexState::Pending;
    rows[3].xactionIndexState = CHIron::Gui::XactionIndexState::Denied;

    CHIron::Gui::FlitTableModel model;
    model.setRows(std::move(rows));
    model.setXactionDeniedOnlyFilter(true);

    expectEqual(model.visibleCount(),
                2,
                QStringLiteral("Denied flit filter should show only rows marked denied by the Xaction index."));
    const CHIron::Gui::FlitRecord* firstDenied = model.recordAt(0);
    const CHIron::Gui::FlitRecord* secondDenied = model.recordAt(1);
    expect(firstDenied && firstDenied->xactionIndexState == CHIron::Gui::XactionIndexState::Denied,
           QStringLiteral("First denied-filter row should be denied."));
    expect(secondDenied && secondDenied->xactionIndexState == CHIron::Gui::XactionIndexState::Denied,
           QStringLiteral("Second denied-filter row should be denied."));

    model.setChannelVisible(CHIron::Gui::FlitChannel::Rsp, false);
    expectEqual(model.visibleCount(),
                1,
                QStringLiteral("Denied flit filter should compose with channel filters."));
    const CHIron::Gui::FlitRecord* remaining = model.recordAt(0);
    expect(remaining && remaining->channel == CHIron::Gui::FlitChannel::Dat,
           QStringLiteral("Composed filters should retain only the denied DAT row."));

    model.setXactionDeniedOnlyFilter(false);
    expectEqual(model.visibleCount(),
                3,
                QStringLiteral("Disabling denied flit filter should restore rows allowed by the other filters."));
}

void testDeniedFlitFilterMatchesRedXactionBulletsForTraceSession()
{
    using XactTestConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 256, true, true, true>;
    using ReqFlit = CHI::Eb::Flits::REQ<XactTestConfig>;
    using DatFlit = CHI::Eb::Flits::DAT<XactTestConfig>;

    CLog::Parameters params;
    params.SetIssue(CLog::Issue::Eb);
    expect(params.SetNodeIdWidth(11), QStringLiteral("Failed to set test NodeIDWidth."));
    expect(params.SetReqAddrWidth(52), QStringLiteral("Failed to set test ReqAddrWidth."));
    expect(params.SetReqRSVDCWidth(32), QStringLiteral("Failed to set test ReqRSVDCWidth."));
    expect(params.SetDatRSVDCWidth(32), QStringLiteral("Failed to set test DatRSVDCWidth."));
    expect(params.SetDataWidth(256), QStringLiteral("Failed to set test DataWidth."));
    params.SetDataCheckPresent(true);
    params.SetPoisonPresent(true);
    params.SetMPAMPresent(true);

    const auto serializeReq = [](const ReqFlit& flit, TestFlitBitWriter& writer) {
        appendEbReqFlitForTest(flit, writer);
    };
    const auto serializeDat = [](const DatFlit& flit, TestFlitBitWriter& writer) {
        appendEbDatFlitForTest(flit, writer);
    };

    ReqFlit readReq{};
    readReq.TgtID() = 2;
    readReq.SrcID() = 1;
    readReq.TxnID() = 7;
    readReq.Opcode() = CHI::Eb::Opcodes::REQ::ReadNoSnp;
    readReq.Size() = CHI::Eb::Sizes::B64;
    readReq.Addr() = 0x1000;
    readReq.AllowRetry() = 1;

    DatFlit compData{};
    compData.TgtID() = 1;
    compData.SrcID() = 2;
    compData.TxnID() = 7;
    compData.HomeNID() = 2;
    compData.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
    compData.RespErr() = CHI::Eb::RespErrs::OK;
    compData.Resp() = CHI::Eb::Resps::CompData_UC;
    compData.DBID() = 9;
    compData.BE() = 0xFFFFFFFFU;

    ReqFlit deniedReq = readReq;
    deniedReq.TxnID() = 11;

    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    const QString tracePath = tempDir.filePath(QStringLiteral("denied_filter_xaction.clogb"));
    writeTraceWithTopology(tracePath,
                           params,
                           {
                               {CLog::NodeType::RN_F, 1},
                               {CLog::NodeType::HN_F, 2},
                           },
                           {
                               makeSerializedRecord(CLog::Channel::TXREQ, 1, 1, readReq, serializeReq),
                               makeSerializedRecord(CLog::Channel::RXDAT, 2, 1, compData, serializeDat),
                               makeSerializedRecord(CLog::Channel::TXREQ, 1, 1, deniedReq, serializeReq),
                           });

    CHIron::Gui::CLogBTraceLoadError error;
    std::shared_ptr<CHIron::Gui::TraceSession> session;
    expect(CHIron::Gui::TraceSession::open(tracePath, session, error),
           error.summary.isEmpty()
               ? QStringLiteral("Trace session should open the denied-filter trace.")
               : error.summary);
    expect(session != nullptr, QStringLiteral("Trace session should be created."));
    expect(session->buildXactionIndex(error),
           error.summary.isEmpty()
               ? QStringLiteral("Persisted Xaction index should build for the denied-filter trace.")
               : error.summary);

    CHIron::Gui::FlitTableModel model;
    model.setTraceSession(session);
    model.setXactionDeniedOnlyFilter(true);
    for (int iteration = 0; iteration < 200 && model.visibleCount() == 3; ++iteration) {
        QApplication::processEvents();
    }

    expectEqual(model.visibleCount(),
                1,
                QStringLiteral("Denied flit filter should retain only rows whose Xaction bullet is red."));
    const CHIron::Gui::FlitRecord* deniedRow = model.recordAt(0);
    expect(deniedRow && deniedRow->xactionIndexState == CHIron::Gui::XactionIndexState::Denied,
           QStringLiteral("The filtered session row should have the same Denied state used by the red Xaction bullet."));

    const QModelIndex xactionIndex = model.index(0, CHIron::Gui::FlitTableModel::XactionIndexColumn);
    expect(xactionIndex.data(CHIron::Gui::FlitTableModel::XactionIndexStateRole).toInt()
               == static_cast<int>(CHIron::Gui::XactionIndexState::Denied),
           QStringLiteral("The filtered row's Xaction role should render the red bullet."));
}

void testErrorsDockShowsDeniedXactionRows()
{
    resetErrorsDispositionSettings();

    using XactTestConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 256, true, true, true>;
    using ReqFlit = CHI::Eb::Flits::REQ<XactTestConfig>;
    using DatFlit = CHI::Eb::Flits::DAT<XactTestConfig>;

    CLog::Parameters params;
    params.SetIssue(CLog::Issue::Eb);
    expect(params.SetNodeIdWidth(11), QStringLiteral("Failed to set error-dock NodeIDWidth."));
    expect(params.SetReqAddrWidth(52), QStringLiteral("Failed to set error-dock ReqAddrWidth."));
    expect(params.SetReqRSVDCWidth(32), QStringLiteral("Failed to set error-dock ReqRSVDCWidth."));
    expect(params.SetDatRSVDCWidth(32), QStringLiteral("Failed to set error-dock DatRSVDCWidth."));
    expect(params.SetDataWidth(256), QStringLiteral("Failed to set error-dock DataWidth."));
    params.SetDataCheckPresent(true);
    params.SetPoisonPresent(true);
    params.SetMPAMPresent(true);

    const auto serializeReq = [](const ReqFlit& flit, TestFlitBitWriter& writer) {
        appendEbReqFlitForTest(flit, writer);
    };
    const auto serializeDat = [](const DatFlit& flit, TestFlitBitWriter& writer) {
        appendEbDatFlitForTest(flit, writer);
    };

    ReqFlit readReq{};
    readReq.TgtID() = 2;
    readReq.SrcID() = 1;
    readReq.TxnID() = 7;
    readReq.Opcode() = CHI::Eb::Opcodes::REQ::ReadNoSnp;
    readReq.Size() = CHI::Eb::Sizes::B64;
    readReq.Addr() = 0x1000;
    readReq.AllowRetry() = 1;

    DatFlit compData{};
    compData.TgtID() = 1;
    compData.SrcID() = 2;
    compData.TxnID() = 7;
    compData.HomeNID() = 2;
    compData.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
    compData.RespErr() = CHI::Eb::RespErrs::OK;
    compData.Resp() = CHI::Eb::Resps::CompData_UC;
    compData.DBID() = 9;
    compData.BE() = 0xFFFFFFFFU;

    ReqFlit deniedReq = readReq;
    deniedReq.TxnID() = 11;

    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    const QString tracePath = tempDir.filePath(QStringLiteral("errors_dock_denied_xaction.clogb"));
    writeTraceWithTopology(tracePath,
                           params,
                           {
                               {CLog::NodeType::RN_F, 1},
                               {CLog::NodeType::HN_F, 2},
                           },
                           {
                               makeSerializedRecord(CLog::Channel::TXREQ, 1, 1, readReq, serializeReq),
                               makeSerializedRecord(CLog::Channel::RXDAT, 2, 1, compData, serializeDat),
                               makeSerializedRecord(CLog::Channel::TXREQ, 1, 1, deniedReq, serializeReq),
                           });

    CHIron::Gui::MainWindow window;
    window.resize(1200, 720);
    window.show();
    QApplication::processEvents();
    expect(window.testLoadTraceFile(tracePath),
           QStringLiteral("Errors dock denied-row trace should load."));
    window.testStartXactionIndexing(true);
    expect(waitForCondition([&window]() {
               return !window.testSessionXactionIndexActive(0);
           }, 5000),
           QStringLiteral("Errors dock denied-row trace should finish xaction indexing."));
    expectEqual(window.testActivePersistedTraceIssueCount(),
                1,
                QStringLiteral("Active Errors dock session should read the persisted red-bullet issue table."));

    CHIron::Gui::CLogBTraceLoadError sessionError;
    std::shared_ptr<CHIron::Gui::TraceSession> session;
    expect(CHIron::Gui::TraceSession::open(tracePath, session, sessionError),
           sessionError.summary.isEmpty()
               ? QStringLiteral("Errors dock denied-row comparison session should open.")
               : sessionError.summary);
    expect(session != nullptr,
           QStringLiteral("Errors dock denied-row comparison session should be created."));
    expect(session->buildXactionIndex(sessionError),
           sessionError.summary.isEmpty()
               ? QStringLiteral("Errors dock denied-row comparison index should build.")
               : sessionError.summary);
    std::vector<CHIron::Gui::TraceIssue> persistedIssues;
    expect(session->xactionIndexIssues(persistedIssues, sessionError),
           sessionError.summary.isEmpty()
               ? QStringLiteral("Errors dock denied-row issue table should be readable.")
               : sessionError.summary);
    expectEqual(static_cast<int>(persistedIssues.size()),
                1,
                QStringLiteral("Errors dock denied-row issue table should persist the red-bullet row."));
    CHIron::Gui::FlitTableModel model;
    model.setTraceSession(session);
    model.setXactionDeniedOnlyFilter(true);
    expect(waitForCondition([&model]() {
               return model.visibleCount() == 1;
           }, 5000),
           QStringLiteral("Errors dock comparison model should finish the denied-only filter build."));
    expectEqual(model.visibleCount(),
                1,
                QStringLiteral("Errors dock comparison model should find one red-bullet denied row."));
    const CHIron::Gui::FlitRecord* deniedRow = model.recordAt(0);
    expect(deniedRow != nullptr,
           QStringLiteral("Errors dock comparison model should expose the denied row."));
    const int expectedDeniedRow = model.logicalRowAt(0) + 1;
    expect(expectedDeniedRow > 0,
           QStringLiteral("Errors dock comparison row should have a logical row."));

    window.testSetErrorSeverityVisible(CHIron::Gui::TraceIssueSeverity::Error, false);
    window.testSetErrorSeverityVisible(CHIron::Gui::TraceIssueSeverity::Warning, false);
    window.testShowErrorsDock();
    expect(waitForCondition([&window]() {
               return window.testErrorsBuildComplete()
                   && !window.testErrorsBuildActive()
                   && window.testErrorWidgetErrorCount() == 1
                   && !window.testErrorWidgetProgressVisible();
           }, 10000),
           QStringLiteral("Errors dock should finish loading and update toolbar counts when all severities are hidden: ")
               + window.testErrorsBuildDebugState());
    expectEqual(window.testErrorIssueCount(),
                0,
                QStringLiteral("Hidden severities should keep the Errors list empty after loading finishes."));
    expectEqual(window.testErrorWidgetErrorCount(),
                1,
                QStringLiteral("Errors widget toolbar count should update even when the loaded error row is hidden."));
    expect(window.testErrorSeverityButtonText(CHIron::Gui::TraceIssueSeverity::Error)
               .contains(QStringLiteral("1 error")),
           QStringLiteral("Errors widget Error button should retain the loaded count while errors are hidden."));
    expect(!window.testErrorWidgetStatusText().contains(QStringLiteral("Loading persisted errors")),
           QStringLiteral("Errors widget should clear loading text after hidden-row loading finishes."));
    expect(window.testErrorWidgetStatusText().contains(QStringLiteral("No visible issues")),
           QStringLiteral("Errors widget should summarize hidden loaded issues as filtered, not absent."));
    expect(!window.testErrorWidgetProgressVisible(),
           QStringLiteral("Errors widget progress bar should hide after hidden-row loading finishes."));

    window.testSetErrorSeverityVisible(CHIron::Gui::TraceIssueSeverity::Error, true);
    window.testSetErrorSeverityVisible(CHIron::Gui::TraceIssueSeverity::Warning, true);
    QApplication::processEvents();
    expect(waitForCondition([&window]() {
               return window.testErrorsBuildComplete()
                   && !window.testErrorsBuildActive()
                   && window.testErrorErrorCount() == 1;
           }, 10000),
           QStringLiteral("Errors dock should finish collecting denied xaction rows: ")
               + window.testErrorsBuildDebugState());
    expectEqual(window.testErrorErrorCount(),
                1,
                QStringLiteral("Errors dock should show the row rendered as a red denied Xaction bullet."));
    expectEqual(window.testErrorWarningCount(),
                0,
                QStringLiteral("Errors dock should reserve warning count for future diagnostics."));
    expect(window.testErrorIssueCodeAt(0).startsWith(QStringLiteral("0x")),
           QStringLiteral("Errors dock should expose the persisted numeric denial code."));
    expectEqual(window.testErrorIssueSourceAt(0),
                QStringLiteral("Xaction"),
                QStringLiteral("Errors dock should expose the persisted issue source."));
    expectEqual(window.testErrorIssueRowTextAt(0),
                QString::number(expectedDeniedRow),
                QStringLiteral("Errors dock should expose the one-based persisted trace row."));
    expect(window.testErrorIssueSummaryAt(0).contains(QStringLiteral("row %1").arg(expectedDeniedRow)),
           QStringLiteral("Errors dock description should point at the same denied trace row as the red bullet."));
    expect(window.testErrorIssueDetailsAt(0).contains(QStringLiteral("Xaction index denial")),
           QStringLiteral("Errors dock details should include Xaction denial context."));
    window.testSetErrorIssueDisposition(CHIron::Gui::TraceIssueSource::XactionIndex,
                                        CHIron::Gui::TraceIssueDisposition::Warning);
    QApplication::processEvents();
    expectEqual(window.testErrorErrorCount(),
                0,
                QStringLiteral("Changing Xaction disposition to Warning should clear error count."));
    expectEqual(window.testErrorWarningCount(),
                1,
                QStringLiteral("Changing Xaction disposition to Warning should move the persisted issue to warnings."));
    window.testSetErrorSeverityVisible(CHIron::Gui::TraceIssueSeverity::Warning, false);
    QApplication::processEvents();
    expectEqual(window.testErrorIssueCount(),
                0,
                QStringLiteral("Hiding warnings should hide warning-classified Xaction rows."));
    expectEqual(window.testErrorWarningCount(),
                1,
                QStringLiteral("Hiding warnings should keep the warning count available in the toolbar."));
    window.testSetErrorSeverityVisible(CHIron::Gui::TraceIssueSeverity::Warning, true);
    QApplication::processEvents();
    expectEqual(window.testErrorIssueCount(),
                1,
                QStringLiteral("Showing warnings should restore warning-classified Xaction rows."));

    CHIron::Gui::MainWindow reopenedWindow;
    reopenedWindow.resize(1200, 720);
    reopenedWindow.show();
    QApplication::processEvents();
    expect(reopenedWindow.testLoadTraceFile(tracePath),
           QStringLiteral("Errors dock denied-row trace should reopen with the persisted index."));
    reopenedWindow.testShowErrorsDock();
    expect(waitForCondition([&reopenedWindow]() {
               return reopenedWindow.testErrorsBuildComplete()
                   && !reopenedWindow.testErrorsBuildActive();
           }, 5000),
           QStringLiteral("Errors dock should load persisted issue records after reopening."));
    expectEqual(reopenedWindow.testErrorErrorCount(),
                0,
                QStringLiteral("Reopened Errors dock should restore Xaction Warning disposition from settings."));
    expectEqual(reopenedWindow.testErrorWarningCount(),
                1,
                QStringLiteral("Reopened Errors dock should keep the persisted issue as a warning."));
    expectEqual(reopenedWindow.testErrorIssueDisposition(CHIron::Gui::TraceIssueSource::XactionIndex),
                QStringLiteral("Warning"),
                QStringLiteral("Reopened Errors dock should restore Xaction disposition control."));
    expect(reopenedWindow.testErrorIssueCodeAt(0).startsWith(QStringLiteral("0x")),
           QStringLiteral("Reopened Errors dock should keep the persisted numeric denial code."));
    expectEqual(reopenedWindow.testErrorIssueSourceAt(0),
                QStringLiteral("Xaction"),
                QStringLiteral("Reopened Errors dock should keep the persisted issue source."));
    expectEqual(reopenedWindow.testErrorIssueRowTextAt(0),
                QString::number(expectedDeniedRow),
                QStringLiteral("Reopened Errors dock should keep the persisted trace row."));
    expect(reopenedWindow.testErrorIssueSummaryAt(0).contains(QStringLiteral("row %1").arg(expectedDeniedRow)),
           QStringLiteral("Reopened Errors dock description should keep the persisted denied trace row."));
    expect(!reopenedWindow.testErrorIssueSummaryAt(0).contains(QStringLiteral("DENIED_XACTION")),
           QStringLiteral("Persisted issue rows should not use synthetic fallback denial names."));
    expect(!reopenedWindow.testErrorIssueExpanded(0),
           QStringLiteral("Reopened persisted Errors issue should start collapsed."));
    expect(reopenedWindow.testSetErrorIssueExpanded(0, true),
           QStringLiteral("Reopened persisted Errors issue should expand from persisted details."));
    expect(reopenedWindow.testExpandedErrorIssueDetailsAt(0).contains(QStringLiteral("Xaction index denial")),
           QStringLiteral("Reopened persisted Errors detail row should show persisted denial context."));
    resetErrorsDispositionSettings();
}

void testDoubleClickTransactionHighlightMarksRelatedRows()
{
    std::vector<CHIron::Gui::FlitRecord> rows = buildFilterTestRows();
    rows[0].transactionGroupKey = QStringLiteral("xaction|1");
    rows[1].transactionGroupKey = QStringLiteral("xaction|1");
    rows[2].transactionGroupKey = QStringLiteral("xaction|1");
    rows[3].transactionGroupKey = QStringLiteral("xaction|2");

    CHIron::Gui::FlitTableModel model;
    model.setRows(std::move(rows));
    expect(model.toggleTransactionHighlightFromVisibleRow(1),
           QStringLiteral("First double-click should enable transaction highlighting."));

    const QColor clickedBackground =
        model.data(model.index(1, CHIron::Gui::FlitTableModel::OpcodeColumn),
                   Qt::BackgroundRole)
            .value<QColor>();
    const QColor relatedBackground =
        model.data(model.index(2, CHIron::Gui::FlitTableModel::OpcodeColumn),
                   Qt::BackgroundRole)
            .value<QColor>();
    const QColor unrelatedBackground =
        model.data(model.index(3, CHIron::Gui::FlitTableModel::OpcodeColumn),
                   Qt::BackgroundRole)
            .value<QColor>();

    expect(clickedBackground == QColor(QStringLiteral("#FF8A00")),
           QStringLiteral("Double-click transaction highlight should use bright orange."));
    expect(relatedBackground == clickedBackground,
           QStringLiteral("Rows with the same transaction group should share the orange highlight."));
    expect(unrelatedBackground != clickedBackground,
           QStringLiteral("Rows from a different transaction group should not get the orange highlight."));
    expect(model.isTransactionHighlightedRow(0),
           QStringLiteral("The transaction anchor's related request should be highlighted."));
    expect(!model.isTransactionHighlightedRow(3),
           QStringLiteral("Unrelated rows should not report transaction highlighting."));

    expect(!model.toggleTransactionHighlightFromVisibleRow(1),
           QStringLiteral("Second double-click on the anchor row should clear transaction highlighting."));
    const QColor clearedBackground =
        model.data(model.index(1, CHIron::Gui::FlitTableModel::OpcodeColumn),
                   Qt::BackgroundRole)
            .value<QColor>();
    expect(clearedBackground != QColor(QStringLiteral("#FF8A00")),
           QStringLiteral("Clearing transaction highlighting should remove the orange background."));

    expect(model.toggleTransactionHighlightFromVisibleRow(2),
           QStringLiteral("Double-clicking a related row after clearing should enable highlighting again."));
    expect(model.isTransactionHighlightAnchorRow(2),
           QStringLiteral("The latest double-clicked row should become the transaction highlight anchor."));
    expect(model.toggleTransactionHighlightFromVisibleRow(1),
           QStringLiteral("Double-clicking a different related row should move the anchor, not clear."));
    expect(model.isTransactionHighlightAnchorRow(1),
           QStringLiteral("The transaction highlight anchor should move to the newly double-clicked row."));
}

void testReadNoSnpTransactionHighlightIncludesAllCompDataBeats()
{
    using XactTestConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 256, true, true, true>;
    using ReqFlit = CHI::Eb::Flits::REQ<XactTestConfig>;
    using DatFlit = CHI::Eb::Flits::DAT<XactTestConfig>;

    CLog::Parameters params;
    params.SetIssue(CLog::Issue::Eb);
    expect(params.SetNodeIdWidth(11), QStringLiteral("Failed to set test NodeIDWidth."));
    expect(params.SetReqAddrWidth(52), QStringLiteral("Failed to set test ReqAddrWidth."));
    expect(params.SetReqRSVDCWidth(32), QStringLiteral("Failed to set test ReqRSVDCWidth."));
    expect(params.SetDatRSVDCWidth(32), QStringLiteral("Failed to set test DatRSVDCWidth."));
    expect(params.SetDataWidth(256), QStringLiteral("Failed to set test DataWidth."));
    params.SetDataCheckPresent(true);
    params.SetPoisonPresent(true);
    params.SetMPAMPresent(true);

    ReqFlit readReq{};
    readReq.TgtID() = 2;
    readReq.SrcID() = 1;
    readReq.TxnID() = 7;
    readReq.Opcode() = CHI::Eb::Opcodes::REQ::ReadNoSnp;
    readReq.Size() = CHI::Eb::Sizes::B64;
    readReq.Addr() = 0x1000;
    readReq.AllowRetry() = 1;

    DatFlit compData0{};
    compData0.TgtID() = 1;
    compData0.SrcID() = 2;
    compData0.TxnID() = 7;
    compData0.HomeNID() = 2;
    compData0.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
    compData0.RespErr() = CHI::Eb::RespErrs::OK;
    compData0.Resp() = CHI::Eb::Resps::CompData_UC;
    compData0.DBID() = 9;
    compData0.DataID() = 0;
    compData0.BE() = 0xFFFFFFFFU;
    compData0.Data()[0] = 0x1112131415161718ULL;
    compData0.Data()[1] = 0x2122232425262728ULL;
    compData0.Data()[2] = 0x3132333435363738ULL;
    compData0.Data()[3] = 0x4142434445464748ULL;

    DatFlit compData2 = compData0;
    compData2.DataID() = 2;
    compData2.Data()[0] = 0x5152535455565758ULL;
    compData2.Data()[1] = 0x6162636465666768ULL;
    compData2.Data()[2] = 0x7172737475767778ULL;
    compData2.Data()[3] = 0x8182838485868788ULL;

    const auto serializeReq = [](const ReqFlit& flit, TestFlitBitWriter& writer) {
        appendEbReqFlitForTest(flit, writer);
    };
    const auto serializeDat = [](const DatFlit& flit, TestFlitBitWriter& writer) {
        appendEbDatFlitForTest(flit, writer);
    };

    CLog::CLogB::TagCHIRecords block;
    block.head.timeBase = 0;
    block.records.push_back(makeSerializedRecord(CLog::Channel::TXREQ,
                                                 1,
                                                 1,
                                                 readReq,
                                                 serializeReq));
    block.records.push_back(makeSerializedRecord(CLog::Channel::RXDAT,
                                                 1,
                                                 1,
                                                 compData0,
                                                 serializeDat));
    block.records.push_back(makeSerializedRecord(CLog::Channel::RXDAT,
                                                 1,
                                                 1,
                                                 compData2,
                                                 serializeDat));

    CHIron::Gui::CLogBTraceMetadata metadata;
    metadata.parameters = params;
    metadata.nodeTypes[1] = CLog::NodeType::RN_F;
    metadata.nodeTypes[2] = CLog::NodeType::HN_F;
    metadata.blocks.push_back(CHIron::Gui::CLogBTraceBlockSummary{
        .serializedSize = 1,
        .recordCount = static_cast<std::uint32_t>(block.records.size()),
    });
    metadata.totalRecords = block.records.size();

    std::vector<CHIron::Gui::FlitRecord> rows;
    CHIron::Gui::CLogBTraceLoadError error;
    expect(CHIron::Gui::CLogBTraceLoader::decodeBlockRows(metadata,
                                                          0,
                                                          block,
                                                          0,
                                                          block.records.size(),
                                                          rows,
                                                          error,
                                                          {},
                                                          {},
                                                          false),
           error.summary.isEmpty()
               ? QStringLiteral("ReadNoSnp test block should decode successfully.")
               : error.summary);
    expectEqual(rows.size(),
                static_cast<std::size_t>(3),
                QStringLiteral("ReadNoSnp test block should decode all records."));
    expect(rows[0].opcode == QStringLiteral("ReadNoSnp"),
           QStringLiteral("First decoded row should be the ReadNoSnp request."));
    expect(rows[1].opcode == QStringLiteral("CompData"),
           QStringLiteral("Second decoded row should be the first CompData beat."));
    expect(rows[2].opcode == QStringLiteral("CompData"),
           QStringLiteral("Third decoded row should be the second CompData beat."));
    expect(rows[0].xactionDebugLog.isEmpty(),
           QStringLiteral("Random-access row decoding should not emit transient Xaction debug logs."));

    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    const QString tracePath = tempDir.filePath(QStringLiteral("read_no_snp_xaction.clogb"));
    writeTraceWithTopology(tracePath,
                           params,
                           {
                               {CLog::NodeType::RN_F, 1},
                               {CLog::NodeType::HN_F, 2},
                           },
                           std::move(block.records));

    std::shared_ptr<CHIron::Gui::TraceSession> session;
    expect(CHIron::Gui::TraceSession::open(tracePath, session, error),
           error.summary.isEmpty()
               ? QStringLiteral("Trace session should open the ReadNoSnp test trace.")
               : error.summary);
    expect(session != nullptr, QStringLiteral("Trace session should be created."));
    const bool readNoSnpIndexBuilt = session->buildXactionIndex(error);
    expect(readNoSnpIndexBuilt,
           error.summary.isEmpty()
               ? QStringLiteral("Persisted Xaction index should build for ReadNoSnp test trace.")
               : error.summary);

    std::vector<CHIron::Gui::FlitRecord> indexedRows;
    expect(session->readRows(0, 3, indexedRows, error),
           error.summary.isEmpty()
               ? QStringLiteral("Indexed ReadNoSnp rows should read back from the session.")
               : error.summary);
    const QString xactionKey = firstXactionTransactionKey(indexedRows[0]);
    expect(!xactionKey.isEmpty(),
           QStringLiteral("ReadNoSnp request should receive a persisted Xaction transaction key."));
    expect(CHIron::Gui::TransactionGroupKeys(indexedRows[1]).contains(xactionKey),
           QStringLiteral("First CompData beat should share the persisted ReadNoSnp Xaction key."));
    expect(CHIron::Gui::TransactionGroupKeys(indexedRows[2]).contains(xactionKey),
           QStringLiteral("Second CompData beat should share the persisted ReadNoSnp Xaction key."));

    const QString requestDebug = session->xactionDebugInfo(0);
    const QString dataDebug = session->xactionDebugInfo(2);
    expect(requestDebug.contains(QStringLiteral("Joint path: RNFJoint::NextTXREQ")),
           QStringLiteral("Persisted request debug info should include its RNF joint path."));
    expect(requestDebug.contains(QStringLiteral("Denial: XACT_ACCEPTED")),
           QStringLiteral("Persisted request debug info should preserve the Xaction denial."));
    expect(dataDebug.contains(QStringLiteral("Joint path: RNFJoint::NextRXDAT")),
           QStringLiteral("Persisted data debug info should include its RNF joint path."));
    expect(dataDebug.contains(QStringLiteral("Xaction type:")),
           QStringLiteral("Persisted data debug info should preserve the Xaction type."));

    const std::vector<std::uint64_t> relatedRows = session->xactionRowsForGroup(xactionKey);
    expectVectorEqual(relatedRows,
                      {0, 1, 2},
                      QStringLiteral("Persisted Xaction-to-rows map should return related rows."));
}

void testXactionIndexPersistsRowsAfterChunkWriteFlush()
{
    using XactTestConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 256, true, true, true>;
    using ReqFlit = CHI::Eb::Flits::REQ<XactTestConfig>;
    using DatFlit = CHI::Eb::Flits::DAT<XactTestConfig>;

    CLog::Parameters params;
    params.SetIssue(CLog::Issue::Eb);
    expect(params.SetNodeIdWidth(11), QStringLiteral("Failed to set test NodeIDWidth."));
    expect(params.SetReqAddrWidth(52), QStringLiteral("Failed to set test ReqAddrWidth."));
    expect(params.SetReqRSVDCWidth(32), QStringLiteral("Failed to set test ReqRSVDCWidth."));
    expect(params.SetDatRSVDCWidth(32), QStringLiteral("Failed to set test DatRSVDCWidth."));
    expect(params.SetDataWidth(256), QStringLiteral("Failed to set test DataWidth."));
    params.SetDataCheckPresent(true);
    params.SetPoisonPresent(true);
    params.SetMPAMPresent(true);

    const auto serializeReq = [](const ReqFlit& flit, TestFlitBitWriter& writer) {
        appendEbReqFlitForTest(flit, writer);
    };
    const auto serializeDat = [](const DatFlit& flit, TestFlitBitWriter& writer) {
        appendEbDatFlitForTest(flit, writer);
    };

    constexpr std::size_t kRowsPastFirstChunkFlush = 262144;
    constexpr std::size_t kXactionBeginRow = kRowsPastFirstChunkFlush;
    std::vector<CLog::CLogB::TagCHIRecords::Record> records;
    records.reserve(kXactionBeginRow + 3);
    for (std::size_t index = 0; index < kXactionBeginRow; ++index) {
        records.push_back(makeLengthOnlyRecord(CLog::Channel::TXREQ, storedByteLengthForTest(ReqFlit::WIDTH)));
    }

    ReqFlit readReq{};
    readReq.TgtID() = 2;
    readReq.SrcID() = 1;
    readReq.TxnID() = 7;
    readReq.Opcode() = CHI::Eb::Opcodes::REQ::ReadNoSnp;
    readReq.Size() = CHI::Eb::Sizes::B64;
    readReq.Addr() = 0x1000;
    readReq.AllowRetry() = 1;

    DatFlit compData{};
    compData.TgtID() = 1;
    compData.SrcID() = 2;
    compData.TxnID() = 7;
    compData.HomeNID() = 2;
    compData.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
    compData.RespErr() = CHI::Eb::RespErrs::OK;
    compData.Resp() = CHI::Eb::Resps::CompData_UC;
    compData.DBID() = 9;
    compData.BE() = 0xFFFFFFFFU;
    compData.Data()[0] = 0x1112131415161718ULL;
    compData.Data()[1] = 0x2122232425262728ULL;
    compData.Data()[2] = 0x3132333435363738ULL;
    compData.Data()[3] = 0x4142434445464748ULL;

    DatFlit compData2 = compData;
    compData2.DataID() = 2;
    compData2.Data()[0] = 0x5152535455565758ULL;

    records.push_back(makeSerializedRecord(CLog::Channel::TXREQ, 1, 1, readReq, serializeReq));
    records.push_back(makeSerializedRecord(CLog::Channel::RXDAT, 1, 1, compData, serializeDat));
    records.push_back(makeSerializedRecord(CLog::Channel::RXDAT, 1, 1, compData2, serializeDat));

    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    const QString tracePath = tempDir.filePath(QStringLiteral("xaction_flush_boundary.clogb"));
    writeTraceWithTopology(tracePath,
                           params,
                           {
                               {CLog::NodeType::RN_F, 1},
                               {CLog::NodeType::HN_F, 2},
                           },
                           std::move(records));

    CHIron::Gui::CLogBTraceLoadError error;
    std::shared_ptr<CHIron::Gui::TraceSession> session;
    expect(CHIron::Gui::TraceSession::open(tracePath, session, error),
           error.summary.isEmpty()
               ? QStringLiteral("Trace session should open the xaction flush-boundary trace.")
               : error.summary);
    expect(session != nullptr, QStringLiteral("Trace session should be created."));
    expect(session->buildXactionIndex(error),
           error.summary.isEmpty()
               ? QStringLiteral("Persisted Xaction index should build for the flush-boundary trace.")
               : error.summary);

    std::vector<CHIron::Gui::FlitRecord> indexedRows;
    expect(session->readRows(kXactionBeginRow, 3, indexedRows, error),
           error.summary.isEmpty()
               ? QStringLiteral("Rows past the xaction row-map flush boundary should read back.")
               : error.summary);
    expectEqual(indexedRows.size(),
                static_cast<std::size_t>(3),
                QStringLiteral("Flush-boundary read should return the three xaction rows."));
    const QString xactionKey = firstXactionTransactionKey(indexedRows[0]);
    expect(!xactionKey.isEmpty(),
           QStringLiteral("Xaction row after the chunk writer flush boundary should keep its persisted key."));
    expect(CHIron::Gui::TransactionGroupKeys(indexedRows[1]).contains(xactionKey),
           QStringLiteral("First data beat after the chunk writer flush boundary should share the xaction key."));
    expect(CHIron::Gui::TransactionGroupKeys(indexedRows[2]).contains(xactionKey),
           QStringLiteral("Second data beat after the chunk writer flush boundary should share the xaction key."));
}

void testBeforeSamRequestChannelsDisplayAsReqWithMarker()
{
    using XactTestConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 256, true, true, true>;
    using ReqFlit = CHI::Eb::Flits::REQ<XactTestConfig>;
    using DatFlit = CHI::Eb::Flits::DAT<XactTestConfig>;

    CLog::Parameters params;
    params.SetIssue(CLog::Issue::Eb);
    expect(params.SetNodeIdWidth(11), QStringLiteral("Failed to set test NodeIDWidth."));
    expect(params.SetReqAddrWidth(52), QStringLiteral("Failed to set test ReqAddrWidth."));
    expect(params.SetReqRSVDCWidth(32), QStringLiteral("Failed to set test ReqRSVDCWidth."));
    expect(params.SetDatRSVDCWidth(32), QStringLiteral("Failed to set test DatRSVDCWidth."));
    expect(params.SetDataWidth(256), QStringLiteral("Failed to set test DataWidth."));
    params.SetDataCheckPresent(true);
    params.SetPoisonPresent(true);
    params.SetMPAMPresent(true);

    ReqFlit txReq{};
    txReq.TgtID() = 2;
    txReq.SrcID() = 1;
    txReq.TxnID() = 7;
    txReq.Opcode() = CHI::Eb::Opcodes::REQ::ReadNoSnp;
    txReq.Size() = CHI::Eb::Sizes::B64;
    txReq.Addr() = 0x1000;
    txReq.AllowRetry() = 1;

    ReqFlit rxReq = txReq;
    rxReq.TgtID() = 4;
    rxReq.SrcID() = 1;
    rxReq.TxnID() = 8;
    rxReq.Addr() = 0x2000;

    DatFlit txCompData{};
    txCompData.TgtID() = 1;
    txCompData.SrcID() = 2;
    txCompData.TxnID() = 7;
    txCompData.HomeNID() = 2;
    txCompData.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
    txCompData.RespErr() = CHI::Eb::RespErrs::OK;
    txCompData.Resp() = CHI::Eb::Resps::CompData_UC;
    txCompData.DBID() = 9;
    txCompData.DataID() = 0;
    txCompData.BE() = 0xFFFFFFFFU;

    const auto serializeReq = [](const ReqFlit& flit, TestFlitBitWriter& writer) {
        appendEbReqFlitForTest(flit, writer);
    };
    const auto serializeDat = [](const DatFlit& flit, TestFlitBitWriter& writer) {
        appendEbDatFlitForTest(flit, writer);
    };

    CLog::CLogB::TagCHIRecords block;
    block.head.timeBase = 0;
    block.records.push_back(makeSerializedRecord(CLog::Channel::TXREQ_BeforeSAM,
                                                 1,
                                                 1,
                                                 txReq,
                                                 serializeReq));
    block.records.push_back(makeSerializedRecord(CLog::Channel::RXDAT,
                                                 1,
                                                 1,
                                                 txCompData,
                                                 serializeDat));
    block.records.push_back(makeSerializedRecord(CLog::Channel::RXREQ_BeforeSAM,
                                                 3,
                                                 1,
                                                 rxReq,
                                                 serializeReq));

    CHIron::Gui::CLogBTraceMetadata metadata;
    metadata.parameters = params;
    metadata.nodeTypes[1] = CLog::NodeType::RN_F;
    metadata.nodeTypes[2] = CLog::NodeType::HN_F;
    metadata.nodeTypes[3] = CLog::NodeType::SN_F;
    metadata.blocks.push_back(CHIron::Gui::CLogBTraceBlockSummary{
        .serializedSize = 1,
        .recordCount = static_cast<std::uint32_t>(block.records.size()),
    });
    metadata.totalRecords = block.records.size();

    std::vector<CHIron::Gui::FlitRecord> rows;
    CHIron::Gui::CLogBTraceLoadError error;
    expect(CHIron::Gui::CLogBTraceLoader::decodeBlockRows(metadata,
                                                          0,
                                                          block,
                                                          0,
                                                          block.records.size(),
                                                          rows,
                                                          error,
                                                          {},
                                                          {},
                                                          false),
           error.summary.isEmpty()
               ? QStringLiteral("BeforeSAM request block should decode successfully.")
               : error.summary);
    expectEqual(rows.size(),
                static_cast<std::size_t>(3),
                QStringLiteral("BeforeSAM test block should decode all records."));

    expect(rows[0].channel == CHIron::Gui::FlitChannel::Req,
           QStringLiteral("TXREQ_BeforeSAM should display as logical REQ."));
    expect(rows[0].direction == CHIron::Gui::FlitDirection::Tx,
           QStringLiteral("TXREQ_BeforeSAM should display as TX."));
    expect(rows[2].channel == CHIron::Gui::FlitChannel::Req,
           QStringLiteral("RXREQ_BeforeSAM should display as logical REQ."));
    expect(rows[2].direction == CHIron::Gui::FlitDirection::Rx,
           QStringLiteral("RXREQ_BeforeSAM should display as RX."));
    expect(rows[0].channelTag == QStringLiteral("Before SAM")
               && rows[2].channelTag == QStringLiteral("Before SAM"),
           QStringLiteral("BeforeSAM rows should carry the channel badge tag."));
    expect(rows[0].dimTarget && rows[2].dimTarget,
           QStringLiteral("BeforeSAM rows should dim the target column."));
    expect(rows[0].xactionDebugLog.isEmpty(),
           QStringLiteral("Random-access BeforeSAM decoding should not emit transient Xaction debug logs."));

    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    const QString tracePath = tempDir.filePath(QStringLiteral("before_sam_xaction.clogb"));
    writeTraceWithTopology(tracePath,
                           params,
                           {
                               {CLog::NodeType::RN_F, 1},
                               {CLog::NodeType::HN_F, 2},
                               {CLog::NodeType::SN_F, 3},
                           },
                           std::move(block.records));

    std::shared_ptr<CHIron::Gui::TraceSession> session;
    expect(CHIron::Gui::TraceSession::open(tracePath, session, error),
           error.summary.isEmpty()
               ? QStringLiteral("Trace session should open the BeforeSAM test trace.")
               : error.summary);
    expect(session != nullptr, QStringLiteral("Trace session should be created."));
    const bool beforeSamIndexBuilt = session->buildXactionIndex(error);
    expect(beforeSamIndexBuilt,
           error.summary.isEmpty()
               ? QStringLiteral("Persisted Xaction index should build for BeforeSAM test trace.")
               : error.summary);

    std::vector<CHIron::Gui::FlitRecord> indexedRows;
    expect(session->readRows(0, 3, indexedRows, error),
           error.summary.isEmpty()
               ? QStringLiteral("Indexed BeforeSAM rows should read back from the session.")
               : error.summary);
    const QString beforeSamXactionKey = firstXactionTransactionKey(indexedRows[0]);
    expect(!beforeSamXactionKey.isEmpty(),
           QStringLiteral("TXREQ_BeforeSAM should receive a persisted Xaction key."));
    expect(CHIron::Gui::TransactionGroupKeys(indexedRows[1]).contains(beforeSamXactionKey),
           QStringLiteral("BeforeSAM request should still highlight related response flits."));

    const QString txReqDebug = session->xactionDebugInfo(0);
    const QString rxReqDebug = session->xactionDebugInfo(2);
    expect(txReqDebug.contains(QStringLiteral("Joint path: RNFJoint::NextTXREQ"))
               && txReqDebug.contains(QStringLiteral("SAM: BeforeSAM")),
           QStringLiteral("Persisted TXREQ_BeforeSAM debug info should preserve the RNF joint path."));
    expect(rxReqDebug.contains(QStringLiteral("Joint path: SNFJoint::NextRXREQ"))
               && rxReqDebug.contains(QStringLiteral("SAM: BeforeSAM")),
           QStringLiteral("Persisted RXREQ_BeforeSAM debug info should preserve the SNF joint path."));
    expect(!txReqDebug.contains(QStringLiteral("DENIED_REQ_FIELD_TGTID")),
           QStringLiteral("Persisted BeforeSAM RNFJoint tracking should not reject the request by TgtID."));
    expect(!rxReqDebug.contains(QStringLiteral("DENIED_REQ_FIELD_TGTID")),
           QStringLiteral("Persisted BeforeSAM SNFJoint tracking should not reject the request by TgtID."));

    CHIron::Gui::FlitTableModel model;
    model.setRows(rows);
    expectEqual(model.rowCount(),
                3,
                QStringLiteral("REQ filters should include BeforeSAM request rows by default."));
    expect(model.data(model.index(0, CHIron::Gui::FlitTableModel::ChannelColumn),
                      Qt::DisplayRole).toString() == QStringLiteral("REQ"),
           QStringLiteral("BeforeSAM channel display text should stay REQ."));
    expect(model.data(model.index(0, CHIron::Gui::FlitTableModel::ChannelColumn),
                      CHIron::Gui::FlitTableModel::ChannelTagRole).toString().isEmpty(),
           QStringLiteral("BeforeSAM channel tag role should not expose a channel-column badge label."));
    expect(model.data(model.index(0, CHIron::Gui::FlitTableModel::TargetColumn),
                      CHIron::Gui::FlitTableModel::NodeLabelTextRole).toString()
               == QStringLiteral("Before SAM"),
           QStringLiteral("BeforeSAM target tag role should expose the target-column badge label."));
    expect(model.data(model.index(0, CHIron::Gui::FlitTableModel::TargetColumn),
                      Qt::ForegroundRole).value<QColor>().isValid(),
           QStringLiteral("BeforeSAM target column should provide a dim foreground color."));

    model.setDirectionVisible(CHIron::Gui::FlitDirection::Tx, false);
    expectEqual(model.rowCount(),
                2,
                QStringLiteral("Hiding TX should leave RXDAT and RXREQ_BeforeSAM visible."));
    expect(model.recordAt(1)->direction == CHIron::Gui::FlitDirection::Rx,
           QStringLiteral("RXREQ_BeforeSAM should obey the RX direction filter."));

    model.setDirectionVisible(CHIron::Gui::FlitDirection::Tx, true);
    model.setChannelVisible(CHIron::Gui::FlitChannel::Req, false);
    expectEqual(model.rowCount(),
                1,
                QStringLiteral("Hiding REQ should hide BeforeSAM request rows while leaving DAT rows."));
}

void testSnpTargetColumnDisplaysCaptureNodeAndStaysDimmed()
{
    using XactTestConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 256, true, true, true>;
    using SnpFlit = CHI::Eb::Flits::SNP<XactTestConfig>;

    CLog::Parameters params;
    params.SetIssue(CLog::Issue::Eb);
    expect(params.SetNodeIdWidth(11), QStringLiteral("Failed to set test NodeIDWidth."));
    expect(params.SetReqAddrWidth(52), QStringLiteral("Failed to set test ReqAddrWidth."));
    expect(params.SetReqRSVDCWidth(32), QStringLiteral("Failed to set test ReqRSVDCWidth."));
    expect(params.SetDatRSVDCWidth(32), QStringLiteral("Failed to set test DatRSVDCWidth."));
    expect(params.SetDataWidth(256), QStringLiteral("Failed to set test DataWidth."));
    params.SetDataCheckPresent(true);
    params.SetPoisonPresent(true);
    params.SetMPAMPresent(true);

    SnpFlit snp{};
    snp.SrcID() = 10;
    snp.TxnID() = 21;
    snp.FwdNID() = 31;
    snp.FwdTxnID() = 44;
    snp.Opcode() = CHI::Eb::Opcodes::SNP::SnpMakeInvalid;
    snp.Addr() = 0x3000 >> 3U;

    const auto serializeSnp = [](const SnpFlit& flit, TestFlitBitWriter& writer) {
        appendEbSnpFlitForTest(flit, writer);
    };

    CLog::CLogB::TagCHIRecords block;
    block.head.timeBase = 0;
    block.records.push_back(makeSerializedRecord(CLog::Channel::RXSNP,
                                                 7,
                                                 1,
                                                 snp,
                                                 serializeSnp));

    CHIron::Gui::CLogBTraceMetadata metadata;
    metadata.parameters = params;
    metadata.nodeTypes[7] = CLog::NodeType::RN_F;
    metadata.nodeTypes[10] = CLog::NodeType::HN_F;
    metadata.nodeTypes[31] = CLog::NodeType::RN_I;
    metadata.blocks.push_back(CHIron::Gui::CLogBTraceBlockSummary{
        .serializedSize = 1,
        .recordCount = static_cast<std::uint32_t>(block.records.size()),
    });
    metadata.totalRecords = block.records.size();

    std::vector<CHIron::Gui::FlitRecord> rows;
    CHIron::Gui::CLogBTraceLoadError error;
    expect(CHIron::Gui::CLogBTraceLoader::decodeBlockRows(metadata,
                                                          0,
                                                          block,
                                                          0,
                                                          block.records.size(),
                                                          rows,
                                                          error,
                                                          {},
                                                          {},
                                                          false),
           error.summary.isEmpty()
               ? QStringLiteral("SNP target display block should decode successfully.")
               : error.summary);
    expectEqual(rows.size(),
                static_cast<std::size_t>(1),
                QStringLiteral("SNP target display test block should decode one row."));

    const CHIron::Gui::FlitRecord& row = rows.front();
    expect(row.channel == CHIron::Gui::FlitChannel::Snp,
           QStringLiteral("Decoded SNP row should keep the logical SNP channel."));
    expectEqual(row.target,
                QStringLiteral("7"),
                QStringLiteral("SNP TgtID display should use the capturing node id, not FwdNID."));
    expect(row.dimTarget, QStringLiteral("SNP TgtID display should be dimmed."));
    expect(row.channelNodeId && *row.channelNodeId == 7,
           QStringLiteral("SNP row should keep its capture node id metadata."));
    expectEqual(CHIron::Gui::FlitEditAdapter::fieldNameForColumn(
                    CHIron::Gui::FlitTableModel::TargetColumn,
                    QString(),
                    row),
                QStringLiteral("TgtID"),
                QStringLiteral("SNP target table column should not alias to FwdNID."));

    const CHIron::Gui::FlitEditCellState targetState =
        CHIron::Gui::FlitEditAdapter::cellState(row, &metadata, QStringLiteral("TgtID"));
    expect(!targetState.applicable,
           QStringLiteral("SNP TgtID edit state should stay inapplicable so the cell is grayed out."));

    CHIron::Gui::FlitTableModel model;
    model.setRows(rows);
    model.setTraceMetadataOverride(metadata);
    model.setNodeLabelsVisible(true);

    const QModelIndex targetIndex = model.index(0, CHIron::Gui::FlitTableModel::TargetColumn);
    expectEqual(model.data(targetIndex, Qt::DisplayRole).toString(),
                QStringLiteral("7"),
                QStringLiteral("SNP table target cell should display the capture node id."));
    expect(model.data(targetIndex, Qt::ForegroundRole).value<QColor>().isValid(),
           QStringLiteral("SNP target column should provide a dim foreground color."));
    expect(model.data(targetIndex, Qt::BackgroundRole).value<QColor>().isValid(),
           QStringLiteral("SNP target column should provide a dim background color."));
    expectEqual(model.data(targetIndex,
                           CHIron::Gui::FlitTableModel::NodeLabelTextRole).toString(),
                QStringLiteral("RN-F"),
                QStringLiteral("SNP capture-node target should still resolve node labels from metadata."));
}

void testSuggestedFilterWorkerCountIsBounded()
{
    const std::size_t tinyCount = CHIron::Gui::Detail::suggestedFilterWorkerCount(0);
    expectEqual(tinyCount,
                static_cast<std::size_t>(1),
                QStringLiteral("Small filter scans should stay single-threaded."));

    const std::size_t hugeCount = CHIron::Gui::Detail::suggestedFilterWorkerCount(
        (std::numeric_limits<std::size_t>::max)() / 4);
    expect(hugeCount >= 1, QStringLiteral("Filter worker count must always be positive."));
    expect(hugeCount <= 8, QStringLiteral("Filter worker count must be capped at 8."));
}

void testFastIndexDescriptorValidationRejectsCorruption()
{
    namespace Detail = CHIron::Gui::Detail;

    expectEqual(static_cast<std::size_t>(Detail::kFastIndexHeaderSerializedBytes),
                static_cast<std::size_t>(36),
                QStringLiteral("Serialized fast-index header size should stay stable."));
    expectEqual(static_cast<std::size_t>(Detail::kFastRecordSerializedBytes),
                static_cast<std::size_t>(41),
                QStringLiteral("Serialized fast-record size should include timestamp data."));
    expectEqual(static_cast<std::size_t>(Detail::fastIndexDescriptorOffset(0)),
                static_cast<std::size_t>(Detail::kFastIndexHeaderSerializedBytes),
                QStringLiteral("First descriptor should begin immediately after the serialized header."));
    expectEqual(static_cast<std::size_t>(Detail::fastIndexDescriptorOffset(1) - Detail::fastIndexDescriptorOffset(0)),
                static_cast<std::size_t>(Detail::kFastIndexDescriptorSerializedBytes),
                QStringLiteral("Descriptor stride should match the serialized descriptor size."));

    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));

    const QString fastIndexPath = tempDir.filePath(QStringLiteral("trace.fastidx"));
    QFile fastIndexFile(fastIndexPath);
    expect(fastIndexFile.open(QIODevice::WriteOnly), QStringLiteral("Temporary fast-index file creation failed."));
    fastIndexFile.resize(4096);
    fastIndexFile.close();

    CHIron::Gui::CLogBTraceMetadata metadata;
    metadata.blocks.resize(2);
    metadata.blocks[0].recordCount = 4;
    metadata.blocks[1].recordCount = 8;

    std::vector<CHIron::Gui::TraceSession::FastIndexDescriptor> descriptors(2);
    descriptors[0] = {
        .dataOffset = 256,
        .recordCount = 4,
        .dataBytes = static_cast<std::uint64_t>(4 * Detail::kFastRecordSerializedBytes),
    };
    descriptors[1] = {
        .dataOffset = 1024,
        .recordCount = 8,
        .dataBytes = static_cast<std::uint64_t>(8 * Detail::kFastRecordSerializedBytes),
    };

    expect(Detail::validateFastIndexDescriptors(fastIndexPath, metadata, descriptors),
           QStringLiteral("Well-formed fast-index descriptors should validate."));

    descriptors[0].recordCount = 5;
    expect(!Detail::validateFastIndexDescriptors(fastIndexPath, metadata, descriptors),
           QStringLiteral("Descriptor record-count mismatches must be rejected."));

    descriptors[0].recordCount = 4;
    descriptors[0].dataOffset = 8;
    expect(!Detail::validateFastIndexDescriptors(fastIndexPath, metadata, descriptors),
           QStringLiteral("Descriptor offsets inside the descriptor table must be rejected."));
}

void testFastRecordRangeFilterPreservesMatchOrder()
{
    CHIron::Gui::CLogBTraceMetadata metadata;
    metadata.parameters.SetIssue(CLog::Issue::Eb);
    expect(metadata.parameters.SetReqAddrWidth(48),
           QStringLiteral("Failed to configure test CHI parameters."));

    std::vector<CHIron::Gui::CLogBTraceFastRecordInfo> records(25000);
    for (std::size_t index = 0; index < records.size(); ++index) {
        records[index].channel = static_cast<std::uint8_t>(CLog::Channel::TXREQ);
        records[index].sourceId = static_cast<std::uint32_t>(index % 7);
        records[index].address = static_cast<std::uint64_t>(index * 64);
    }

    CHIron::Gui::CLogBTraceFastFilter filter;
    filter.transportMask = CHIron::Gui::CLogBChannelBit(CLog::Channel::TXREQ);
    filter.sourceIdFilter = QStringLiteral("3");

    CHIron::Gui::CLogBTraceLoadError error;
    std::vector<std::size_t> matchedRows;
    const bool ok = CHIron::Gui::CLogBTraceLoader::collectFastRecordRowsMatchingFilterRange(
        metadata,
        filter,
        records,
        100,
        20000,
        matchedRows,
        error);
    expect(ok, QStringLiteral("Fast-record range filtering should succeed."));
    expect(error.summary.isEmpty(), QStringLiteral("Fast-record range filtering should not populate an error."));

    std::vector<std::size_t> expectedRows;
    for (std::size_t index = 100; index < 20100; ++index) {
        if ((index % 7) == 3) {
            expectedRows.push_back(index);
        }
    }

    expectVectorEqual(matchedRows,
                      expectedRows,
                      QStringLiteral("Fast-record range filtering should preserve ascending match order."));
}

void testOptionalFieldIndexProgressUsesRecordCount()
{
    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));

    CLog::Parameters parameters;
    parameters.SetIssue(CLog::Issue::Eb);
    expect(parameters.SetNodeIdWidth(CHIron::Gui::ViewerConfig::nodeIdWidth),
           QStringLiteral("Failed to set test NodeIDWidth."));
    expect(parameters.SetReqAddrWidth(CHIron::Gui::ViewerConfig::reqAddrWidth),
           QStringLiteral("Failed to set test ReqAddrWidth."));
    expect(parameters.SetReqRSVDCWidth(CHIron::Gui::ViewerConfig::reqRsvdcWidth),
           QStringLiteral("Failed to set test ReqRSVDCWidth."));
    expect(parameters.SetDatRSVDCWidth(CHIron::Gui::ViewerConfig::datRsvdcWidth),
           QStringLiteral("Failed to set test DatRSVDCWidth."));
    expect(parameters.SetDataWidth(CHIron::Gui::ViewerConfig::dataWidth),
           QStringLiteral("Failed to set test DataWidth."));
    parameters.SetDataCheckPresent(CHIron::Gui::ViewerConfig::dataCheckWidth != 0);
    parameters.SetPoisonPresent(CHIron::Gui::ViewerConfig::poisonWidth != 0);
    parameters.SetMPAMPresent(CHIron::Gui::ViewerConfig::mpamWidth != 0);

    const std::uint8_t reqBytes = storedByteLengthForTest(CHIron::Gui::ReqFlit::WIDTH);
    const std::uint8_t rspBytes = storedByteLengthForTest(CHIron::Gui::RspFlit::WIDTH);
    const std::uint8_t datBytes = storedByteLengthForTest(CHIron::Gui::DatFlit::WIDTH);
    const std::uint8_t snpBytes = storedByteLengthForTest(CHIron::Gui::SnpFlit::WIDTH);

    const QString tracePath = tempDir.filePath(QStringLiteral("optional_index_progress.clogb"));
    writeLengthOnlyTraceBlocks(tracePath,
                               parameters,
                               {
                                   {
                                       makeLengthOnlyRecord(CLog::Channel::TXREQ, reqBytes),
                                       makeLengthOnlyRecord(CLog::Channel::TXRSP, rspBytes),
                                       makeLengthOnlyRecord(CLog::Channel::TXDAT, datBytes),
                                   },
                                   {
                                       makeLengthOnlyRecord(CLog::Channel::TXSNP, snpBytes),
                                       makeLengthOnlyRecord(CLog::Channel::RXREQ, reqBytes),
                                       makeLengthOnlyRecord(CLog::Channel::RXDAT, datBytes),
                                   },
                               });

    std::shared_ptr<CHIron::Gui::TraceSession> session;
    CHIron::Gui::CLogBTraceLoadError error;
    expect(CHIron::Gui::TraceSession::open(tracePath, session, error),
           QStringLiteral("Trace session should open the test CLog.B file."));
    expect(session != nullptr, QStringLiteral("Trace session should be created."));

    std::vector<std::pair<std::size_t, std::size_t>> progress;
    CHIron::Gui::CLogBTraceLoadCallbacks callbacks;
    callbacks.stageProgress = [&progress](const std::size_t completed, const std::size_t total) {
        progress.push_back({completed, total});
    };

    expect(session->buildOptionalFieldFastIndex(QStringLiteral("TraceTag"), error, callbacks),
           QStringLiteral("Optional field fast index creation should succeed."));
    expect(session->hasOptionalFieldFastIndex(QStringLiteral("TraceTag")),
           QStringLiteral("Optional field fast index should be available after creation."));
    expect(!progress.empty(), QStringLiteral("Optional field indexing should report progress."));

    const auto finalProgress = progress.back();
    expectEqual(finalProgress.first,
                static_cast<std::size_t>(6),
                QStringLiteral("Optional field indexing should report completed records."));
    expectEqual(finalProgress.second,
                static_cast<std::size_t>(6),
                QStringLiteral("Optional field indexing progress total should be records, not blocks."));

    CHIron::Gui::CLogBTraceFastFilter fastFilter;
    CHIron::Gui::CLogBTraceLoader::prepareFastFilter(fastFilter);
    QHash<QString, QString> optionalFilters;
    optionalFilters.insert(QStringLiteral("TraceTag"), QStringLiteral("0"));

    std::vector<int> matchedRows;
    expect(session->collectRowsMatchingFastFilterAndOptionalFieldIndexesRange(0,
                                                                              fastFilter,
                                                                              optionalFilters,
                                                                              0,
                                                                              3,
                                                                              matchedRows,
                                                                              error),
           QStringLiteral("Optional TraceTag fast index filtering should succeed for the first block."));
    expectEqual(static_cast<int>(matchedRows.size()),
                3,
                QStringLiteral("Optional TraceTag filter should match every zero TraceTag row in the first block."));
    expectEqual(matchedRows[0], 0, QStringLiteral("First optional filter match should be row 0."));
    expectEqual(matchedRows[1], 1, QStringLiteral("Second optional filter match should be row 1."));
    expectEqual(matchedRows[2], 2, QStringLiteral("Third optional filter match should be row 2."));

    matchedRows.clear();
    expect(session->collectRowsMatchingFastFilterAndOptionalFieldIndexesRange(1,
                                                                              fastFilter,
                                                                              optionalFilters,
                                                                              0,
                                                                              3,
                                                                              matchedRows,
                                                                              error),
           QStringLiteral("Optional TraceTag fast index filtering should succeed for the second block."));
    expectEqual(static_cast<int>(matchedRows.size()),
                3,
                QStringLiteral("Optional TraceTag filter should match every zero TraceTag row in the second block."));
    expectEqual(matchedRows[0], 3, QStringLiteral("First second-block optional filter match should be row 3."));
    expectEqual(matchedRows[1], 4, QStringLiteral("Second second-block optional filter match should be row 4."));
    expectEqual(matchedRows[2], 5, QStringLiteral("Third second-block optional filter match should be row 5."));
}

void testOptionalFieldIndexCanBeClearedAndRebuilt()
{
    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));

    CLog::Parameters parameters;
    parameters.SetIssue(CLog::Issue::Eb);
    expect(parameters.SetNodeIdWidth(CHIron::Gui::ViewerConfig::nodeIdWidth),
           QStringLiteral("Failed to set test NodeIDWidth."));
    expect(parameters.SetReqAddrWidth(CHIron::Gui::ViewerConfig::reqAddrWidth),
           QStringLiteral("Failed to set test ReqAddrWidth."));
    expect(parameters.SetReqRSVDCWidth(CHIron::Gui::ViewerConfig::reqRsvdcWidth),
           QStringLiteral("Failed to set test ReqRSVDCWidth."));
    expect(parameters.SetDatRSVDCWidth(CHIron::Gui::ViewerConfig::datRsvdcWidth),
           QStringLiteral("Failed to set test DatRSVDCWidth."));
    expect(parameters.SetDataWidth(CHIron::Gui::ViewerConfig::dataWidth),
           QStringLiteral("Failed to set test DataWidth."));
    parameters.SetDataCheckPresent(CHIron::Gui::ViewerConfig::dataCheckWidth != 0);
    parameters.SetPoisonPresent(CHIron::Gui::ViewerConfig::poisonWidth != 0);
    parameters.SetMPAMPresent(CHIron::Gui::ViewerConfig::mpamWidth != 0);

    const std::uint8_t reqBytes = storedByteLengthForTest(CHIron::Gui::ReqFlit::WIDTH);
    const std::uint8_t rspBytes = storedByteLengthForTest(CHIron::Gui::RspFlit::WIDTH);
    const QString tracePath = tempDir.filePath(QStringLiteral("optional_index_clear_rebuild.clogb"));
    writeLengthOnlyTrace(tracePath,
                         parameters,
                         {
                             makeLengthOnlyRecord(CLog::Channel::TXREQ, reqBytes),
                             makeLengthOnlyRecord(CLog::Channel::TXRSP, rspBytes),
                         });

    std::shared_ptr<CHIron::Gui::TraceSession> session;
    CHIron::Gui::CLogBTraceLoadError error;
    expect(CHIron::Gui::TraceSession::open(tracePath, session, error),
           QStringLiteral("Trace session should open the test CLog.B file."));
    expect(session != nullptr, QStringLiteral("Trace session should be created."));

    const QString fieldName = QStringLiteral("TraceTag");
    expect(session->buildOptionalFieldFastIndex(fieldName, error),
           QStringLiteral("Optional field fast index creation should succeed."));
    expect(session->hasOptionalFieldFastIndex(fieldName),
           QStringLiteral("Optional field fast index should be available before clearing."));

    const QString indexPath = session->optionalFieldFastIndexPath(fieldName);
    expect(QFile::exists(indexPath), QStringLiteral("Optional field fast index file should exist before clearing."));
    expect(session->clearOptionalFieldFastIndex(fieldName, error),
           QStringLiteral("Optional field fast index clearing should succeed."));
    expect(!session->hasOptionalFieldFastIndex(fieldName),
           QStringLiteral("Optional field fast index should not be available after clearing."));
    expect(!QFile::exists(indexPath), QStringLiteral("Optional field fast index file should be deleted."));

    expect(session->buildOptionalFieldFastIndex(fieldName, error),
           QStringLiteral("Optional field fast index creation should succeed after clearing."));
    expect(session->hasOptionalFieldFastIndex(fieldName),
           QStringLiteral("Optional field fast index should be available after rebuilding."));
    expect(QFile::exists(indexPath), QStringLiteral("Optional field fast index file should exist after rebuilding."));
}

void testOptionalEbFallbackFieldIndexSupportsEightRecordWorkers()
{
    CHIron::Gui::CLogBTraceMetadata metadata;
    metadata.parameters.SetIssue(CLog::Issue::Eb);
    expect(metadata.parameters.SetNodeIdWidth(11),
           QStringLiteral("Failed to set test NodeIDWidth."));
    expect(metadata.parameters.SetReqAddrWidth(52),
           QStringLiteral("Failed to set test ReqAddrWidth."));
    expect(metadata.parameters.SetReqRSVDCWidth(32),
           QStringLiteral("Failed to set test ReqRSVDCWidth."));
    expect(metadata.parameters.SetDatRSVDCWidth(32),
           QStringLiteral("Failed to set test DatRSVDCWidth."));
    expect(metadata.parameters.SetDataWidth(512),
           QStringLiteral("Failed to set test DataWidth."));
    metadata.parameters.SetDataCheckPresent(true);
    metadata.parameters.SetPoisonPresent(true);
    metadata.parameters.SetMPAMPresent(true);

    constexpr std::size_t recordCount = 8192;
    CLog::CLogB::TagCHIRecords block;
    block.head.timeBase = 0;
    block.records.reserve(recordCount);
    using FlexibleTestConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 512, true, true, true>;
    using FlexibleReqFlit = CHI::Eb::Flits::REQ<FlexibleTestConfig>;
    const std::uint8_t reqBytes = storedByteLengthForTest(FlexibleReqFlit::WIDTH);
    for (std::size_t index = 0; index < recordCount; ++index) {
        block.records.push_back(makeLengthOnlyRecord(CLog::Channel::TXREQ, reqBytes));
    }

    CHIron::Gui::CLogBTraceBlockSummary summary;
    summary.recordCount = static_cast<std::uint32_t>(recordCount);
    metadata.blocks.push_back(summary);
    metadata.totalRecords = recordCount;

    std::vector<CHIron::Gui::CLogBTraceOptionalFieldValue> records;
    CHIron::Gui::CLogBTraceLoadError error;
    expect(CHIron::Gui::CLogBTraceLoader::buildBlockOptionalFieldRecords(
               metadata,
               0,
               block,
               QStringLiteral("QoS"),
               records,
               error,
               8),
           QStringLiteral("E.b fallback optional field indexing should succeed with 8 record workers."));
    expectEqual(records.size(),
                recordCount,
                QStringLiteral("E.b fallback optional indexing should produce one value per record."));
    expect(records.front().present && records.front().value == QStringLiteral("0"),
           QStringLiteral("E.b fallback optional indexing should decode the first value."));
    expect(records.back().present && records.back().value == QStringLiteral("0"),
           QStringLiteral("E.b fallback optional indexing should decode the last value."));
}

void testRowStatisticsSummarizeOpcodeCounts()
{
    const std::vector<CHIron::Gui::FlitRecord> rows = buildFilterTestRows();
    const CHIron::Gui::TraceStatisticsResult statistics = CHIron::Gui::CalculateTraceStatistics(rows);

    expectEqual(static_cast<int>(statistics.totalFlits),
                4,
                QStringLiteral("Row statistics should report the total number of source rows."));
    expectEqual(static_cast<int>(statistics.txFlits),
                1,
                QStringLiteral("Row statistics should count TX records."));
    expectEqual(static_cast<int>(statistics.rxFlits),
                3,
                QStringLiteral("Row statistics should count RX records."));
    expectEqual(static_cast<int>(statistics.channelCounts[0]),
                1,
                QStringLiteral("Row statistics should count REQ records."));
    expectEqual(static_cast<int>(statistics.channelCounts[2]),
                2,
                QStringLiteral("Row statistics should count DAT records."));
    expectEqual(static_cast<int>(statistics.uniqueOpcodeCount),
                3,
                QStringLiteral("Row statistics should keep one entry per direction/channel/opcode combination."));
    expect(!statistics.opcodeCounts.empty(),
           QStringLiteral("Row statistics should include opcode count rows."));
    expect(statistics.opcodeCounts.front().count >= statistics.opcodeCounts.back().count,
           QStringLiteral("Opcode counts should be sorted descending by frequency."));
}

void testTimelineFitZoomAndVisibleRangeShortcuts()
{
    CHIron::Gui::TimelineWidget widget;
    widget.setRows(buildTimelineTestRows());
    showTimelineForTest(widget);

    expect(std::abs(widget.testHorizontalZoom() - 1.0) < 0.001,
           QStringLiteral("Timeline should start fitted to the whole trace."));
    auto range = widget.testVisibleLogicalRowRange();
    expectEqual(static_cast<int>(range.first),
                0,
                QStringLiteral("Fitted timeline should start at logical row 0."));
    expectEqual(static_cast<int>(range.second),
                255,
                QStringLiteral("Fitted timeline should show the last logical row."));
    expect(widget.testVisibleRangeText().contains(QStringLiteral("Time 1,000 - 3,550")),
           QStringLiteral("Timeline visible range should display timestamp thousands separators."));

    sendTimelineWheel(widget, widget.testPlotRect().center(), 120, Qt::ControlModifier);
    expectGreater(widget.testHorizontalZoom(),
                  1.0,
                  QStringLiteral("Ctrl+wheel should zoom in horizontally."));
    range = widget.testVisibleLogicalRowRange();
    expect(range.second - range.first < 255,
           QStringLiteral("Zoomed timeline should show a narrower logical row range."));

    sendTimelineKey(widget, Qt::Key_F);
    expect(std::abs(widget.testHorizontalZoom() - 1.0) < 0.001,
           QStringLiteral("F should fit the whole trace."));
    range = widget.testVisibleLogicalRowRange();
    expectEqual(static_cast<int>(range.first),
                0,
                QStringLiteral("Fit shortcut should reset the visible range start."));
    expectEqual(static_cast<int>(range.second),
                255,
                QStringLiteral("Fit shortcut should include the last row."));

    sendTimelineKey(widget, Qt::Key_Plus, Qt::ControlModifier);
    expectGreater(widget.testHorizontalZoom(),
                  1.0,
                  QStringLiteral("Ctrl++ should zoom in around the viewport center."));
    sendTimelineKey(widget, Qt::Key_0, Qt::ControlModifier);
    expect(std::abs(widget.testHorizontalZoom() - 1.0) < 0.001,
           QStringLiteral("Ctrl+0 should fit the whole trace."));
}

void testTimelineUsesLinearTimestampSpacing()
{
    CHIron::Gui::TimelineWidget widget;
    widget.setRows(buildSparseTimelineTestRows());
    showTimelineForTest(widget);

    const auto row0 = widget.testScreenXForLogicalRow(0);
    const auto row1 = widget.testScreenXForLogicalRow(1);
    const auto row2 = widget.testScreenXForLogicalRow(2);
    const auto row3 = widget.testScreenXForLogicalRow(3);
    expect(row0.has_value() && row1.has_value() && row2.has_value() && row3.has_value(),
           QStringLiteral("Sparse timestamp rows should all be visible at fit zoom."));

    const int earlySpan = *row2 - *row0;
    const int lateGap = *row3 - *row2;
    expect(lateGap > earlySpan * 20,
           QStringLiteral("Timeline x positions should preserve large blank timestamp gaps."));
    expect(*row1 - *row0 <= earlySpan,
           QStringLiteral("Close timestamps should remain grouped in the early part of the axis."));
}

void testTimelineKeepsOpcodeLabelsForTags()
{
    CHIron::Gui::TimelineWidget widget;
    widget.setRows(buildTimelineTestRows());
    showTimelineForTest(widget);

    expectEqual(widget.testOpcodeLabelForLogicalRow(0),
                QStringLiteral("TimelineOp"),
                QStringLiteral("Timeline should keep opcode text for visible tags."));
    expectEqual(widget.testOpcodeLabelForLogicalRow(128),
                QStringLiteral("TimelineOp"),
                QStringLiteral("Timeline should keep opcode text after lane/event sorting."));

    CHIron::Gui::TimelineWidget denseWidget;
    denseWidget.setRows(buildDenseTimelineOpcodeRows());
    showTimelineForTest(denseWidget);
    for (int step = 0; step < 3; ++step) {
        sendTimelineKey(denseWidget, Qt::Key_Plus, Qt::ControlModifier);
    }
    expect(denseWidget.testVisibleOpcodeTagCount() > 0,
           QStringLiteral("Timeline should paint opcode tags at inspection zoom levels."));
    expect(denseWidget.testVisibleOpcodeTagsOverlapFlits(),
           QStringLiteral("Dense same-lane tags should be classified as overlapping flits for translucent painting."));
}

void testTimelineLaneOrderingByNodeAndChannelHeaders()
{
    CHIron::Gui::TimelineWidget widget;
    widget.setRows(buildTimelineTestRows());
    showTimelineForTest(widget);

    expect(widget.testLaneSortOrder() == CHIron::Gui::TimelineLaneSortOrder::NodeThenChannel,
           QStringLiteral("Timeline should default to node-first lane ordering."));
    expectEqual(widget.testLaneKey(0),
                QStringLiteral("0:TXREQ"),
                QStringLiteral("Node-first ordering should group by node ID first."));
    expectEqual(widget.testLaneKey(1),
                QStringLiteral("0:TXDAT"),
                QStringLiteral("Node-first ordering should keep the next channel for the same node."));

    sendTimelineMouse(widget,
                      QEvent::MouseButtonPress,
                      widget.testChannelHeaderRect().center(),
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    sendTimelineMouse(widget,
                      QEvent::MouseButtonRelease,
                      widget.testChannelHeaderRect().center(),
                      Qt::LeftButton,
                      Qt::NoButton,
                      Qt::NoModifier);
    expect(widget.testLaneSortOrder() == CHIron::Gui::TimelineLaneSortOrder::ChannelThenNode,
           QStringLiteral("Clicking Channel header should switch to channel-first lane ordering."));
    expectEqual(widget.testLaneKey(0),
                QStringLiteral("0:TXREQ"),
                QStringLiteral("Channel-first ordering should start with TXREQ."));
    expectEqual(widget.testLaneKey(1),
                QStringLiteral("2:TXREQ"),
                QStringLiteral("Channel-first ordering should keep TXREQ rows grouped before TXDAT."));

    sendTimelineMouse(widget,
                      QEvent::MouseButtonPress,
                      widget.testNodeHeaderRect().center(),
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    sendTimelineMouse(widget,
                      QEvent::MouseButtonRelease,
                      widget.testNodeHeaderRect().center(),
                      Qt::LeftButton,
                      Qt::NoButton,
                      Qt::NoModifier);
    expect(widget.testLaneSortOrder() == CHIron::Gui::TimelineLaneSortOrder::NodeThenChannel,
           QStringLiteral("Clicking Node header should switch back to node-first lane ordering."));
    expectEqual(widget.testLaneKey(1),
                QStringLiteral("0:TXDAT"),
                QStringLiteral("Node-first ordering should be restored after header click."));
}

void testTimelineLaneRowsCanBeDraggedToCustomOrder()
{
    CHIron::Gui::TimelineWidget widget;
    widget.setRows(buildTimelineTestRows());
    showTimelineForTest(widget);

    const QString originalFirst = widget.testLaneKey(0);
    const QString originalThird = widget.testLaneKey(2);
    const QPoint source = widget.testLaneTablePoint(2);
    const QPoint target = widget.testLaneTablePoint(0);

    sendTimelineMouse(widget,
                      QEvent::MouseButtonPress,
                      source,
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    sendTimelineMouse(widget,
                      QEvent::MouseMove,
                      target,
                      Qt::NoButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    sendTimelineMouse(widget,
                      QEvent::MouseButtonRelease,
                      target,
                      Qt::LeftButton,
                      Qt::NoButton,
                      Qt::NoModifier);

    expect(widget.testLaneSortOrder() == CHIron::Gui::TimelineLaneSortOrder::Custom,
           QStringLiteral("Dragging a timeline lane row should switch to custom lane ordering."));
    expectEqual(widget.testLaneKey(0),
                originalThird,
                QStringLiteral("Dragged timeline lane should move to the drop row."));
    expectEqual(widget.testLaneKey(1),
                originalFirst,
                QStringLiteral("Rows between source and drop row should shift down."));

    sendTimelineMouse(widget,
                      QEvent::MouseButtonPress,
                      widget.testNodeHeaderRect().center(),
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    sendTimelineMouse(widget,
                      QEvent::MouseButtonRelease,
                      widget.testNodeHeaderRect().center(),
                      Qt::LeftButton,
                      Qt::NoButton,
                      Qt::NoModifier);
    expect(widget.testLaneSortOrder() == CHIron::Gui::TimelineLaneSortOrder::NodeThenChannel,
           QStringLiteral("Clicking Node header should restore deterministic node-first ordering after a custom drag."));
    expectEqual(widget.testLaneKey(0),
                originalFirst,
                QStringLiteral("Node-first ordering should be restored after custom drag."));
}

void testTimelineCustomLaneOrderPersistsInViewState()
{
    CHIron::Gui::TimelineWidget widget;
    widget.setRows(buildTimelineTestRows());
    showTimelineForTest(widget);

    const QStringList originalOrder = widget.testLaneStateOrder();
    expect(originalOrder.size() > 3,
           QStringLiteral("Timeline test data should expose multiple lanes."));
    widget.testMoveLane(0, originalOrder.size() - 1);
    widget.testMoveLane(1, 0);
    const QStringList customOrder = widget.testLaneStateOrder();
    expect(customOrder != originalOrder,
           QStringLiteral("Test lane moves should produce a custom lane order."));
    expect(widget.testLaneSortOrder() == CHIron::Gui::TimelineLaneSortOrder::Custom,
           QStringLiteral("Programmatic test lane moves should switch to custom order."));

    const QVariantMap state = widget.viewState();
    CHIron::Gui::TimelineWidget restored;
    restored.setRows(buildTimelineTestRows());
    showTimelineForTest(restored);
    restored.restoreViewState(state);
    QApplication::processEvents();

    expect(restored.testLaneSortOrder() == CHIron::Gui::TimelineLaneSortOrder::Custom,
           QStringLiteral("Restored timeline view state should preserve custom lane sort mode."));
    expectEqual(restored.testLaneStateOrder().join(QLatin1Char('|')),
                customOrder.join(QLatin1Char('|')),
                QStringLiteral("Restored timeline view state should preserve the custom lane sequence."));
}

void testTimelineViewStateRestoresSemanticTimeRange()
{
    CHIron::Gui::TimelineWidget source;
    source.setRows(buildTimelineTestRows());
    showTimelineForTest(source);

    QVariantMap state = source.viewState();
    state.insert(QStringLiteral("visibleFirstTimestamp"), 1500);
    state.insert(QStringLiteral("visibleLastTimestamp"), 1900);
    state.insert(QStringLiteral("visibleFirstLogicalRow"), QVariant::fromValue<qulonglong>(50));
    state.insert(QStringLiteral("visibleLastLogicalRow"), QVariant::fromValue<qulonglong>(90));
    state.insert(QStringLiteral("scrollOffset"), QVariant::fromValue<qulonglong>(0));
    source.restoreViewState(state);
    QApplication::processEvents();
    const QString sourceRange = source.testVisibleRangeText();
    expect(sourceRange.contains(QStringLiteral("Time 1,500")),
           QStringLiteral("Timeline semantic time restore should not fall back to raw scroll offset."));

    CHIron::Gui::TimelineWidget shifted;
    shifted.setRows(buildShiftedTimelineTestRows());
    showTimelineForTest(shifted);
    shifted.restoreViewState(state);
    QApplication::processEvents();
    expect(shifted.testVisibleRangeText().contains(QStringLiteral("Time 101,500")),
           QStringLiteral("Timeline should fall back to logical row range when saved timestamps do not belong to this trace."));
}

void testTimelineCursorRemainsAccurateAfterZoomAndPan()
{
    CHIron::Gui::TimelineWidget widget;
    widget.setRows(buildTimelineTestRows());
    showTimelineForTest(widget);

    constexpr int selectedRow = 192;
    widget.setSelectedLogicalRow(selectedRow);
    QApplication::processEvents();
    std::optional<int> initialX = widget.testScreenXForLogicalRow(selectedRow);
    expect(initialX.has_value(),
           QStringLiteral("Timeline cursor should be visible after selecting a trace row."));
    expectRenderedCursorAt(widget,
                           *initialX,
                           QStringLiteral("Rendered cursor should match logical mapping before zoom."));

    for (int step = 0; step < 7; ++step) {
        sendTimelineWheel(widget, widget.testPlotRect().center(), 120, Qt::ControlModifier);
    }
    widget.setSelectedLogicalRow(selectedRow);
    QApplication::processEvents();
    std::optional<int> zoomedX = widget.testScreenXForLogicalRow(selectedRow);
    expect(zoomedX.has_value(),
           QStringLiteral("Timeline cursor should remain visible after zooming."));
    expectRenderedCursorAt(widget,
                           *zoomedX,
                           QStringLiteral("Rendered cursor should use current zoom after trace-table selection."));

    sendTimelineWheel(widget, widget.testPlotRect().center(), -120, Qt::ControlModifier);
    widget.setSelectedLogicalRow(selectedRow);
    QApplication::processEvents();
    std::optional<int> zoomedOutX = widget.testScreenXForLogicalRow(selectedRow);
    expect(zoomedOutX.has_value(),
           QStringLiteral("Timeline cursor should remain visible after zooming back out."));
    expectRenderedCursorAt(widget,
                           *zoomedOutX,
                           QStringLiteral("Rendered cursor should stay aligned after changing zoom levels."));
}

void testTimelineRangeZoomPanMarkerAndLaneSelectionGestures()
{
    CHIron::Gui::TimelineWidget widget;
    widget.setRows(buildTimelineTestRows());
    showTimelineForTest(widget);

    const QRect plot = widget.testPlotRect();
    const QPoint dragStart(plot.left() + plot.width() / 4, plot.center().y());
    const QPoint dragEnd(plot.left() + plot.width() / 2, plot.center().y());
    sendTimelineMouse(widget,
                      QEvent::MouseButtonPress,
                      dragStart,
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    sendTimelineMouse(widget,
                      QEvent::MouseMove,
                      dragEnd,
                      Qt::NoButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    sendTimelineMouse(widget,
                      QEvent::MouseButtonRelease,
                      dragEnd,
                      Qt::LeftButton,
                      Qt::NoButton,
                      Qt::NoModifier);
    expectGreater(widget.testHorizontalZoom(),
                  1.0,
                  QStringLiteral("Plain left-drag should zoom into the selected timeline range."));

    sendTimelineKey(widget, Qt::Key_F);
    widget.setSelectedLogicalRow(0);
    const QPoint clickOnlyPoint(plot.left() + plot.width() / 3, plot.center().y());
    sendTimelineMouse(widget,
                      QEvent::MouseButtonPress,
                      clickOnlyPoint,
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    expectEqual(widget.testSelectedLogicalRow(),
                0,
                QStringLiteral("Left press should not move the cursor before click-vs-drag is known."));
    sendTimelineMouse(widget,
                      QEvent::MouseButtonRelease,
                      clickOnlyPoint,
                      Qt::LeftButton,
                      Qt::NoButton,
                      Qt::NoModifier);
    expect(widget.testSelectedLogicalRow() != 0,
           QStringLiteral("Left click should move the cursor after it is confirmed not to be a drag gesture."));

    widget.setSelectedLogicalRow(0);
    sendTimelineMouse(widget,
                      QEvent::MouseButtonPress,
                      dragStart,
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    expectEqual(widget.testSelectedLogicalRow(),
                0,
                QStringLiteral("Left-drag press should not move the cursor before gesture recognition."));
    sendTimelineMouse(widget,
                      QEvent::MouseMove,
                      dragEnd,
                      Qt::NoButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    sendTimelineMouse(widget,
                      QEvent::MouseButtonRelease,
                      dragEnd,
                      Qt::LeftButton,
                      Qt::NoButton,
                      Qt::NoModifier);
    expectEqual(widget.testSelectedLogicalRow(),
                0,
                QStringLiteral("Left-drag gesture should not move the cursor when it performs zoom."));

    const std::uint64_t offsetBeforePan = widget.testScrollOffset();
    sendTimelineWheel(widget, plot.center(), -120, Qt::ShiftModifier);
    expect(widget.testScrollOffset() > offsetBeforePan,
           QStringLiteral("Shift+wheel should pan horizontally."));

    sendTimelineKey(widget, Qt::Key_Plus, Qt::ControlModifier);
    expectGreater(widget.testHorizontalZoom(),
                  1.0,
                  QStringLiteral("Timeline should be zoomed before testing vertical left-drag zoom-full."));
    const int verticalBeforePlainDrag = widget.testVerticalScrollOffset();
    const QPoint verticalDragStart(plot.center().x(), plot.top() + 12);
    const QPoint verticalDragEnd(plot.center().x() + 2, plot.top() + 96);
    sendTimelineMouse(widget,
                      QEvent::MouseButtonPress,
                      verticalDragStart,
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    sendTimelineMouse(widget,
                      QEvent::MouseMove,
                      verticalDragEnd,
                      Qt::NoButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    sendTimelineMouse(widget,
                      QEvent::MouseButtonRelease,
                      verticalDragEnd,
                      Qt::LeftButton,
                      Qt::NoButton,
                      Qt::NoModifier);
    expectEqual(widget.testVerticalScrollOffset(),
                verticalBeforePlainDrag,
                QStringLiteral("Plain vertical left-drag should not scroll the timeline."));
    expect(std::abs(widget.testHorizontalZoom() - 1.0) < 0.001,
           QStringLiteral("Plain vertical left-drag should zoom full."));

    sendTimelineKey(widget, Qt::Key_F);
    for (int step = 0; step < 4; ++step) {
        sendTimelineKey(widget, Qt::Key_Plus, Qt::ControlModifier);
    }
    const double zoomBeforeDiagonal = widget.testHorizontalZoom();
    const QPoint diagonalDragStart(plot.left() + 32, plot.top() + 18);
    const QPoint diagonalDragEnd(diagonalDragStart.x() + 72, diagonalDragStart.y() + 66);
    sendTimelineMouse(widget,
                      QEvent::MouseButtonPress,
                      diagonalDragStart,
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    sendTimelineMouse(widget,
                      QEvent::MouseMove,
                      QPoint(diagonalDragStart.x() + 12, diagonalDragStart.y() + 1),
                      Qt::NoButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    sendTimelineMouse(widget,
                      QEvent::MouseMove,
                      diagonalDragEnd,
                      Qt::NoButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    sendTimelineMouse(widget,
                      QEvent::MouseButtonRelease,
                      diagonalDragEnd,
                      Qt::LeftButton,
                      Qt::NoButton,
                      Qt::NoModifier);
    expectNear(widget.testHorizontalZoom(),
               zoomBeforeDiagonal * 0.5,
               0.01,
               QStringLiteral("Upper-left to lower-right left-drag should zoom out by 2x."));

    sendTimelineKey(widget, Qt::Key_F);
    sendTimelineKey(widget, Qt::Key_Plus, Qt::ControlModifier);
    const double zoomBeforeReverseDiagonal = widget.testHorizontalZoom();
    const QPoint reverseDiagonalDragStart(plot.left() + 32, plot.top() + 96);
    const QPoint reverseDiagonalDragEnd(reverseDiagonalDragStart.x() + 72,
                                        reverseDiagonalDragStart.y() - 66);
    sendTimelineMouse(widget,
                      QEvent::MouseButtonPress,
                      reverseDiagonalDragStart,
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    sendTimelineMouse(widget,
                      QEvent::MouseMove,
                      QPoint(reverseDiagonalDragStart.x() + 12, reverseDiagonalDragStart.y() - 1),
                      Qt::NoButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    sendTimelineMouse(widget,
                      QEvent::MouseMove,
                      reverseDiagonalDragEnd,
                      Qt::NoButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    sendTimelineMouse(widget,
                      QEvent::MouseButtonRelease,
                      reverseDiagonalDragEnd,
                      Qt::LeftButton,
                      Qt::NoButton,
                      Qt::NoModifier);
    expectNear(widget.testHorizontalZoom(),
               zoomBeforeReverseDiagonal * 2.0,
               0.01,
               QStringLiteral("Lower-left to upper-right left-drag should zoom in by 2x."));

    sendTimelineKey(widget, Qt::Key_F);
    sendTimelineKey(widget, Qt::Key_Plus, Qt::ControlModifier);
    const double zoomBeforeChangedGesture = widget.testHorizontalZoom();
    const QPoint changedDragStart(plot.left() + 32, plot.top() + 110);
    const QPoint changedHorizontalPoint(changedDragStart.x() + 96, changedDragStart.y() + 2);
    const QPoint changedDiagonalEnd(changedDragStart.x() + 80, changedDragStart.y() - 72);
    sendTimelineMouse(widget,
                      QEvent::MouseButtonPress,
                      changedDragStart,
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    sendTimelineMouse(widget,
                      QEvent::MouseMove,
                      changedHorizontalPoint,
                      Qt::NoButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    sendTimelineMouse(widget,
                      QEvent::MouseMove,
                      changedDiagonalEnd,
                      Qt::NoButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    sendTimelineMouse(widget,
                      QEvent::MouseButtonRelease,
                      changedDiagonalEnd,
                      Qt::LeftButton,
                      Qt::NoButton,
                      Qt::NoModifier);
    expectNear(widget.testHorizontalZoom(),
               zoomBeforeChangedGesture * 2.0,
               0.01,
               QStringLiteral("Left-drag should update from range zoom to diagonal zoom-in while moving."));

    sendTimelineKey(widget, Qt::Key_F);
    for (int step = 0; step < 4; ++step) {
        sendTimelineKey(widget, Qt::Key_Plus, Qt::ControlModifier);
    }
    const double zoomBeforeFullRange = widget.testHorizontalZoom();
    const QRect fullRuler(plot.left(), plot.bottom() + 1, plot.width(), 28);
    const QPoint fullDragStart(fullRuler.left() + fullRuler.width() / 20, fullRuler.center().y());
    const QPoint fullDragEnd(fullRuler.left() + fullRuler.width() / 5, fullRuler.center().y());
    sendTimelineMouse(widget,
                      QEvent::MouseButtonPress,
                      fullDragStart,
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    sendTimelineMouse(widget,
                      QEvent::MouseMove,
                      fullDragEnd,
                      Qt::NoButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    sendTimelineMouse(widget,
                      QEvent::MouseButtonRelease,
                      fullDragEnd,
                      Qt::LeftButton,
                      Qt::NoButton,
                      Qt::NoModifier);
    expectGreater(widget.testHorizontalZoom(),
                  zoomBeforeFullRange,
                  QStringLiteral("Plain left-drag on the full ruler should zoom to that full-trace range."));

    widget.setSelectedLogicalRow(128);
    sendTimelineKey(widget, Qt::Key_M);
    expectEqual(widget.testMarkerLogicalRow(),
                128,
                QStringLiteral("M should drop a marker at the current cursor row."));

    const QPoint lanePoint(plot.left() - 12, plot.top() + 8);
    sendTimelineMouse(widget,
                      QEvent::MouseButtonPress,
                      lanePoint,
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    sendTimelineMouse(widget,
                      QEvent::MouseButtonRelease,
                      lanePoint,
                      Qt::LeftButton,
                      Qt::NoButton,
                      Qt::NoModifier);
    expectEqual(widget.testSelectedLane(),
                0,
           QStringLiteral("Clicking a timeline table row should select that lane."));
}

void testAddressWidgetBuildsReqSnpAddressEvents()
{
    CHIron::Gui::AddressWidget widget;
    widget.setRows(buildAddressWidgetTestRows());
    showAddressForTest(widget);

    expectEqual(widget.testEventCount(),
                4,
                QStringLiteral("Address widget should include only addressed REQ/SNP rows."));
    expect(widget.testKindForLogicalRow(1) == CHIron::Gui::AddressEventKind::Read,
           QStringLiteral("Read request opcodes should use the read address color class."));
    expect(widget.testKindForLogicalRow(4) == CHIron::Gui::AddressEventKind::Writeback,
           QStringLiteral("Writeback request opcodes should use the eviction/writeback color class."));
    expect(widget.testKindForLogicalRow(7) == CHIron::Gui::AddressEventKind::Snoop,
           QStringLiteral("SNP flits should use the snoop color class."));
    expect(widget.testKindForLogicalRow(10) == CHIron::Gui::AddressEventKind::Writeback,
           QStringLiteral("Eviction requests should use the eviction/writeback color class."));

    const auto range = widget.testAddressRange();
    expectEqual(static_cast<int>(range.first),
                0x1000,
                QStringLiteral("Address widget should track the minimum plotted address."));
    expectEqual(static_cast<int>(range.second),
                0x8000,
                QStringLiteral("Address widget should track the maximum plotted address."));

    const auto lowY = widget.testScreenYForAddress(0x1000);
    const auto highY = widget.testScreenYForAddress(0x8000);
    expect(lowY.has_value() && highY.has_value(),
           QStringLiteral("Address widget should map addresses to vertical coordinates."));
    expect(*highY < *lowY,
           QStringLiteral("Higher addresses should appear higher on the linear vertical axis."));
}

void testAddressWidgetZoomSnapAndSelectionNavigation()
{
    CHIron::Gui::AddressWidget widget;
    widget.setRows(buildAddressWidgetTestRows());
    showAddressForTest(widget);

    auto range = widget.testVisibleLogicalRowRange();
    expectEqual(static_cast<int>(range.first),
                0,
                QStringLiteral("Address widget should start fitted to row 0."));
    expectEqual(static_cast<int>(range.second),
                11,
                QStringLiteral("Address widget should start fitted to the final row."));

    sendAddressWheel(widget, widget.testPlotRect().center(), 120, Qt::ControlModifier);
    expectGreater(widget.testHorizontalZoom(),
                  1.0,
                  QStringLiteral("Ctrl+wheel should zoom the address view horizontally."));
    range = widget.testVisibleLogicalRowRange();
    expect(range.second - range.first < 11,
           QStringLiteral("Zoomed address view should show a narrower row range."));

    widget.setSelectedLogicalRow(7);
    QApplication::processEvents();
    const auto selectedX = widget.testScreenXForLogicalRow(7);
    expect(selectedX.has_value(),
           QStringLiteral("Selecting a logical row should keep it visible in Address."));
    expectEqual(widget.testSelectedLogicalRow(),
                7,
                QStringLiteral("Address widget should store the selected logical row."));

    sendAddressKey(widget, Qt::Key_Left, Qt::ControlModifier);
    expectEqual(widget.testSelectedLogicalRow(),
                4,
                QStringLiteral("Ctrl+Left should jump to the previous address event."));
    sendAddressKey(widget, Qt::Key_Right, Qt::ControlModifier);
    expectEqual(widget.testSelectedLogicalRow(),
                7,
                QStringLiteral("Ctrl+Right should jump to the next address event."));

    sendAddressKey(widget, Qt::Key_F);
    expect(std::abs(widget.testHorizontalZoom() - 1.0) < 0.001,
           QStringLiteral("F should fit the whole address timeline."));
}

void testAddressWidgetUsesLinearTimestampSpacing()
{
    CHIron::Gui::AddressWidget widget;
    widget.setRows(buildSparseAddressWidgetTestRows());
    showAddressForTest(widget);

    const auto early0 = widget.testScreenXForLogicalRow(0);
    const auto early1 = widget.testScreenXForLogicalRow(1);
    const auto late = widget.testScreenXForLogicalRow(2);
    expect(early0.has_value() && early1.has_value() && late.has_value(),
           QStringLiteral("Sparse address timestamp rows should be visible at fit zoom."));

    const int earlyGap = *early1 - *early0;
    const int blankGap = *late - *early1;
    expect(blankGap > earlyGap * 20,
           QStringLiteral("Address widget x positions should preserve large blank timestamp gaps."));
}

void testAddressWidgetCursorPaintsBelowEventRecords()
{
    CHIron::Gui::AddressWidget widget;
    widget.setRows(buildAddressWidgetTestRows());
    showAddressForTest(widget);

    widget.setSelectedLogicalRow(4);
    QApplication::processEvents();
    const auto x = widget.testScreenXForLogicalRow(4);
    const auto y = widget.testScreenYForAddress(0x4000);
    expect(x.has_value() && y.has_value(),
           QStringLiteral("Selected address event should be visible for paint-layer test."));

    const QColor center = renderAddressPixelAt(widget, QPoint(*x, *y));
    expect(center.red() > 160 && center.green() > 80 && center.green() < 170 && center.blue() < 80,
           QStringLiteral("Selected address event dot should paint over the cursor at the overlap point."));
}

void testAddressWidgetCursorAddressTagIsTopLayerAndRightAligned()
{
    CHIron::Gui::AddressWidget widget;
    widget.setRows(buildAddressWidgetTestRows());
    showAddressForTest(widget);

    widget.setSelectedLogicalRow(7);
    QApplication::processEvents();

    const QRect tagRect = widget.testCursorAddressTagRect();
    const QRect axisRect = widget.testAddressAxisRect();
    expect(!tagRect.isEmpty(),
           QStringLiteral("Selected address should have a visible Y-axis address tag."));
    expectEqual(tagRect.right(),
                axisRect.right() - 6,
                QStringLiteral("Cursor address tag should be right aligned in the address axis area."));

    const QColor tagBackground = renderAddressPixelAt(widget, QPoint(tagRect.left() + 3, tagRect.center().y()));
    expect(tagBackground.red() > 230 && tagBackground.green() > 210 && tagBackground.blue() > 150,
           QStringLiteral("Cursor address tag should render on the top layer over axis/grid content."));
}

void testAddressWidgetClickSnapsToNearestAddressEvent()
{
    CHIron::Gui::AddressWidget widget;
    widget.setRows(buildAddressWidgetTestRows());
    showAddressForTest(widget);

    const auto x = widget.testScreenXForLogicalRow(4);
    const auto y = widget.testScreenYForAddress(0x4000);
    expect(x.has_value() && y.has_value(),
           QStringLiteral("Address event test point should be visible."));

    const QPoint clickPoint(*x + 2, *y + 1);
    sendAddressMouse(widget,
                     QEvent::MouseButtonPress,
                     clickPoint,
                     Qt::LeftButton,
                     Qt::LeftButton,
                     Qt::NoModifier);
    sendAddressMouse(widget,
                     QEvent::MouseButtonRelease,
                     clickPoint,
                     Qt::LeftButton,
                     Qt::NoButton,
                     Qt::NoModifier);
    expectEqual(widget.testSelectedLogicalRow(),
                4,
                QStringLiteral("Address click should snap to the nearest plotted address event."));
}

void clickAddressLegend(CHIron::Gui::AddressWidget& widget, const CHIron::Gui::AddressEventKind kind)
{
    const QRect rect = widget.testLegendToggleRect(kind);
    expect(!rect.isEmpty(), QStringLiteral("Address legend toggle should be visible in the test widget."));
    const QPoint point = rect.center();
    sendAddressMouse(widget,
                     QEvent::MouseButtonPress,
                     point,
                     Qt::LeftButton,
                     Qt::LeftButton,
                     Qt::NoModifier);
    sendAddressMouse(widget,
                     QEvent::MouseButtonRelease,
                     point,
                     Qt::LeftButton,
                     Qt::NoButton,
                     Qt::NoModifier);
}

void testAddressWidgetCategoryTogglesFilterEvents()
{
    using CHIron::Gui::AddressEventKind;

    CHIron::Gui::AddressWidget widget;
    widget.setRows(buildAddressWidgetTestRows());
    showAddressForTest(widget);

    expectEqual(widget.testVisibleEventCount(),
                4,
                QStringLiteral("Address widget should initially show every address category."));

    clickAddressLegend(widget, AddressEventKind::Snoop);
    expect(!widget.testKindVisible(AddressEventKind::Snoop),
           QStringLiteral("Clicking the Snoop legend entry should hide snoop events."));
    expectEqual(widget.testVisibleEventCount(),
                3,
                QStringLiteral("Hidden snoop records should be removed from the visible event count."));

    widget.setSelectedLogicalRow(4);
    sendAddressKey(widget, Qt::Key_Right, Qt::ControlModifier);
    expectEqual(widget.testSelectedLogicalRow(),
                10,
                QStringLiteral("Ctrl+Right should skip address events in hidden categories."));

    clickAddressLegend(widget, AddressEventKind::Snoop);
    expect(widget.testKindVisible(AddressEventKind::Snoop),
           QStringLiteral("Clicking the Snoop legend entry again should show snoop events."));
    widget.setSelectedLogicalRow(4);
    sendAddressKey(widget, Qt::Key_Right, Qt::ControlModifier);
    expectEqual(widget.testSelectedLogicalRow(),
                7,
                QStringLiteral("Ctrl+Right should include visible snoop events after re-enabling them."));

    clickAddressLegend(widget, AddressEventKind::Read);
    clickAddressLegend(widget, AddressEventKind::Writeback);
    clickAddressLegend(widget, AddressEventKind::Snoop);
    expectEqual(widget.testVisibleEventCount(),
                0,
                QStringLiteral("Address widget should allow every category to be hidden."));
}

void testAddressWidgetVerticalZoomScrollAndCursorAddress()
{
    CHIron::Gui::AddressWidget widget;
    widget.setRows(buildAddressWidgetTestRows());
    showAddressForTest(widget);

    const auto globalRange = widget.testVisibleAddressRange();
    expect(globalRange.first < 0x1000,
           QStringLiteral("Address 1x vertical view should leave lower margin below the minimum address."));
    expect(globalRange.second > 0x8000,
           QStringLiteral("Address 1x vertical view should leave upper margin above the maximum address."));

    const QRect plot = widget.testPlotRect();
    sendAddressWheel(widget, plot.center(), 120, Qt::ControlModifier | Qt::ShiftModifier);
    expectGreater(widget.testVerticalZoom(),
                  1.0,
                  QStringLiteral("Ctrl+Shift+wheel should zoom the Address widget vertically."));
    const auto zoomedRange = widget.testVisibleAddressRange();
    expect(zoomedRange.second - zoomedRange.first < globalRange.second - globalRange.first,
           QStringLiteral("Vertical zoom should narrow the visible address range."));

    sendAddressWheel(widget, plot.center(), -120, Qt::NoModifier);
    const auto scrolledRange = widget.testVisibleAddressRange();
    expect(scrolledRange.first != zoomedRange.first || scrolledRange.second != zoomedRange.second,
           QStringLiteral("Plain wheel should scroll the Address widget up and down vertically."));

    sendAddressKey(widget, Qt::Key_S, Qt::ControlModifier);
    const int addressY = plot.center().y();
    const std::uint64_t expectedAddress = widget.testAddressAtPlotY(addressY);
    const QPoint clickPoint(plot.left() + plot.width() / 3, addressY);
    sendAddressMouse(widget,
                     QEvent::MouseButtonPress,
                     clickPoint,
                     Qt::LeftButton,
                     Qt::LeftButton,
                     Qt::NoModifier);
    sendAddressMouse(widget,
                     QEvent::MouseButtonRelease,
                     clickPoint,
                     Qt::LeftButton,
                     Qt::NoButton,
                     Qt::NoModifier);
    expect(widget.testSelectedAddress().has_value(),
           QStringLiteral("Address selection should store the selected Y-axis address."));
    expectEqual(static_cast<int>(*widget.testSelectedAddress()),
                static_cast<int>(expectedAddress),
                QStringLiteral("Free address selection should use the clicked Y coordinate."));
}

void testAddressWidgetUsesDensePaintForLargeFullView()
{
    CHIron::Gui::AddressWidget widget;
    widget.setRows(buildDenseAddressWidgetTestRows());
    showAddressForTest(widget);

    expectEqual(widget.testEventCount(),
                25000,
                QStringLiteral("Dense address paint test should build many address events."));
    expect(widget.testUsesDenseEventPaint(),
           QStringLiteral("Address widget should switch to dense aggregated painting for large full views."));

    sendAddressWheel(widget, widget.testPlotRect().center(), 120, Qt::ControlModifier);
    expectGreater(widget.testHorizontalZoom(),
                  1.0,
                  QStringLiteral("Dense paint optimization should not block normal horizontal zoom."));
}

void testAddressWidgetSelectionFillsAreTranslucentBlue()
{
    CHIron::Gui::AddressWidget widget;
    widget.setRows(buildAddressWidgetTestRows());
    showAddressForTest(widget);

    const QRect overview = widget.testOverviewWindowRect();
    expect(!overview.isEmpty(),
           QStringLiteral("Address overview window should be visible for transparency test."));
    widget.testZoomByFactor(2.0, widget.testPlotRect().center());
    QApplication::processEvents();
    const QRect zoomedOverview = widget.testOverviewWindowRect();
    const QRect fullRuler = widget.testFullRulerRect();
    const QPoint overviewPoint(zoomedOverview.center().x(), zoomedOverview.center().y());
    const QPoint rulerPoint(qMin(fullRuler.right() - 2, zoomedOverview.right() + 20),
                            zoomedOverview.center().y());
    expect(!zoomedOverview.contains(rulerPoint),
           QStringLiteral("Address overview comparison point should be outside the zoomed overview window."));
    const QColor overviewFill = renderAddressPixelAt(widget, overviewPoint);
    const QColor rulerBackground = renderAddressPixelAt(widget, rulerPoint);
    expect(overviewFill.blue() >= rulerBackground.blue()
               && overviewFill.red() < rulerBackground.red()
               && overviewFill.green() < rulerBackground.green()
               && overviewFill.red() > 120,
           QStringLiteral("Address full-trace tab should paint a translucent blue fill."));
    sendAddressKey(widget, Qt::Key_F);

    const QRect plot = widget.testPlotRect();
    const QPoint start(plot.left() + 20, plot.center().y());
    const QPoint end(plot.left() + 120, plot.center().y());
    const QPoint fillPoint(plot.left() + 80, plot.center().y() + plot.height() / 4);
    const QColor before = renderAddressPixelAt(widget, fillPoint);
    widget.testBeginRangeZoom(start);
    widget.testUpdateRangeZoom(end);
    QApplication::processEvents();
    const QColor during = renderAddressPixelAt(widget, fillPoint);

    expect(during.blue() > during.red()
               && during.blue() > during.green()
               && during.red() < before.red()
               && during.green() < before.green()
               && during.red() > 180,
           QStringLiteral("Address left-drag selection should paint a translucent blue fill."));
}

void testAddressWidgetHorizontalRangeZoomKeepsCursorRow()
{
    CHIron::Gui::AddressWidget widget;
    widget.setRows(buildAddressWidgetTestRows());
    showAddressForTest(widget);

    widget.setSelectedLogicalRow(10);
    QApplication::processEvents();
    expectEqual(widget.testSelectedLogicalRow(),
                10,
                QStringLiteral("Address range-zoom regression should start from a selected cursor row."));

    const QRect plot = widget.testPlotRect();
    const QPoint start(plot.left() + 24, plot.center().y());
    const QPoint end(plot.left() + 220, plot.center().y() + 1);
    sendAddressMouse(widget,
                     QEvent::MouseButtonPress,
                     start,
                     Qt::LeftButton,
                     Qt::LeftButton,
                     Qt::NoModifier);
    sendAddressMouse(widget,
                     QEvent::MouseMove,
                     end,
                     Qt::NoButton,
                     Qt::LeftButton,
                     Qt::NoModifier);
    sendAddressMouse(widget,
                     QEvent::MouseButtonRelease,
                     end,
                     Qt::LeftButton,
                     Qt::NoButton,
                     Qt::NoModifier);

    expectGreater(widget.testHorizontalZoom(),
                  1.0,
                  QStringLiteral("Horizontal left-drag should still apply an Address range zoom."));
    expectEqual(widget.testSelectedLogicalRow(),
                10,
                QStringLiteral("Address horizontal range zoom should not move the selected cursor row."));
}

void testCacheWidgetUnavailableStatusForRowSnapshots()
{
    CHIron::Gui::CacheWidget widget;
    showCacheForTest(widget);
    widget.setRows(buildAddressWidgetTestRows());
    QApplication::processEvents();

    expect(widget.testStatusText().contains(QStringLiteral("row snapshots"))
               && widget.testStatusText().contains(QStringLiteral("raw CLog.B E.b replay")),
           QStringLiteral("Cache widget should explain why row snapshots cannot produce actual cache-state lifetimes."));
    expectEqual(widget.testSpanCount(),
                0,
                QStringLiteral("Row-snapshot Cache input should not synthesize cache lifetimes."));
}

void testCacheWidgetSyntheticGroupingSelectionAndDensePaint()
{
    CHIron::Gui::CacheWidget widget;
    showCacheForTest(widget);
    widget.testInjectSyntheticSpans(3, 2400);
    QApplication::processEvents();

    expectEqual(widget.testBandCount(),
                3,
                QStringLiteral("Synthetic Cache fixture should create one stacked band per RN."));
    expectEqual(widget.testSpanCount(),
                2400,
                QStringLiteral("Synthetic Cache fixture should create the requested lifetime count."));
    expectEqual(widget.testVisibleBandCount(),
                0,
                QStringLiteral("Cache should start with no visible RN bands until the user adds one."));
    expectEqual(widget.testBandAt(0).value(QStringLiteral("rnNodeId")).toInt(),
                0,
                QStringLiteral("Cache RN bands should be sorted by RN node id."));
    expectEqual(static_cast<int>(widget.testBandAt(1).value(QStringLiteral("spanCount")).toULongLong()),
                800,
                QStringLiteral("Synthetic Cache lifetimes should remain grouped by RN band."));

    const QVariantMap firstSpan = widget.testSpanAt(0);
    expectEqual(firstSpan.value(QStringLiteral("rnBandIndex")).toInt(),
                0,
                QStringLiteral("First synthetic Cache span should live in the first RN band."));
    expectEqual(static_cast<int>(firstSpan.value(QStringLiteral("lineAddress")).toULongLong()),
                0x1000,
                QStringLiteral("Synthetic Cache line addresses should use 64-byte normalized lines."));
    expect(firstSpan.value(QStringLiteral("openEnded")).toBool(),
           QStringLiteral("Synthetic Cache fixture should include open-ended lifetimes."));
    expect(widget.testSpanAt(7).value(QStringLiteral("endStateText")).toString().contains(QStringLiteral("SC, I")),
           QStringLiteral("Cache model should preserve mixed I/non-I state text for alive spans."));

    expect(widget.testSpanVisualRect(0).isEmpty(),
           QStringLiteral("Hidden RN bands should not render span rectangles."));
    expect(widget.testAddVisibleBand(0),
           QStringLiteral("Test fixture should allow adding an RN band to the visible Cache view."));
    expectEqual(widget.testVisibleBandCount(),
                1,
                QStringLiteral("Adding one RN band should expose exactly one visible band."));
    expect(!widget.testAddBandButtonRect().isEmpty(),
           QStringLiteral("Cache header should expose a + button for adding hidden RN bands."));

    widget.testSetHorizontalZoom(200.0);
    widget.testSetHorizontalScroll(0);
    widget.testSetVerticalScroll(0);
    QApplication::processEvents();
    int targetIndex = -1;
    QRect targetRect;
    for (int spanIndex = 0; spanIndex < widget.testSpanCount(); ++spanIndex) {
        const QRect rect = widget.testSpanVisualRect(spanIndex);
        if (!rect.isEmpty() && rect.width() >= 6 && widget.testSpanIndexAtPoint(rect.center()) == spanIndex) {
            targetIndex = spanIndex;
            targetRect = rect;
            break;
        }
    }
    expect(targetIndex >= 0,
           QStringLiteral("Cache fixture should expose at least one unambiguous visible rectangle."));
    const QPoint clickPoint = targetRect.center();
    expectEqual(widget.testSpanIndexAtPoint(clickPoint),
                targetIndex,
                QStringLiteral("Cache hit testing should identify rectangles by point."));
    const int expectedActivatedRow =
        static_cast<int>(widget.testSpanAt(targetIndex).value(QStringLiteral("startLogicalRow")).toULongLong());

    std::vector<int> activatedRows;
    QObject::connect(&widget,
                     &CHIron::Gui::CacheWidget::logicalRowActivated,
                     [&activatedRows](const int logicalRow) {
                         activatedRows.push_back(logicalRow);
                     });
    sendCacheMouse(widget,
                   QEvent::MouseButtonPress,
                   clickPoint,
                   Qt::LeftButton,
                   Qt::LeftButton,
                   Qt::NoModifier);
    sendCacheMouse(widget,
                   QEvent::MouseButtonRelease,
                   clickPoint,
                   Qt::LeftButton,
                   Qt::NoButton,
                   Qt::NoModifier);
    expectEqual(widget.testSelectedSpanIndex(),
                targetIndex,
                QStringLiteral("Clicking a Cache rectangle should select it."));
    expectEqual(widget.testSelectedLogicalRow(),
                expectedActivatedRow,
                QStringLiteral("Clicking a Cache rectangle should select its start logical row."));
    expect(!activatedRows.empty() && activatedRows.back() == expectedActivatedRow,
           QStringLiteral("Cache rectangle activation should emit the start logical row."));

    widget.setSelectedLogicalRow(8);
    QApplication::processEvents();
    expectEqual(widget.testSelectedSpanIndex(),
                2,
                QStringLiteral("Cache selection sync should find the lifetime containing a logical row."));
    sendCacheKey(widget, Qt::Key_M);
    expect(widget.testHasMarker() && widget.testMarkerLogicalRow() == 8,
           QStringLiteral("M should drop a Cache marker at the synchronized row."));
    sendCacheKey(widget, Qt::Key_F);

    widget.testAddVisibleBand(1);
    widget.testAddVisibleBand(2);
    QApplication::processEvents();

    const QImage image = renderCacheImage(widget);
    Q_UNUSED(image);
    expect(widget.testUsesDensePaint(),
           QStringLiteral("Large Cache span sets should use dense paint caching."));
    expect(widget.testDenseTileCacheEntryCount() > 0,
           QStringLiteral("Dense Cache painting should populate reusable image tiles."));

    widget.testSetHorizontalZoom(20.0);
    widget.testSetHorizontalScroll(8000);
    widget.testSetVerticalScroll(40);
    QApplication::processEvents();
    const int candidateCount = widget.testVisiblePaintCandidateCount();
    expect(candidateCount > 0,
           QStringLiteral("Cache paint index should find visible lifetime candidates."));
    expect(candidateCount < widget.testSpanCount() / 2,
           QStringLiteral("Zoomed Cache paint candidates should be viewport bounded."));

    const int tileCountBeforeScroll = widget.testDenseTileCacheEntryCount();
    widget.testSetHorizontalScroll(8050);
    QApplication::processEvents();
    renderCacheImage(widget);
    expect(widget.testDenseTileCacheEntryCount() >= tileCountBeforeScroll,
           QStringLiteral("Cache tile images should survive ordinary horizontal scrolling."));

    expect(widget.testRemoveVisibleBandAt(0),
           QStringLiteral("Cache should allow removing a visible RN band without deleting model spans."));
    expectEqual(widget.testVisibleBandCount(),
                2,
                QStringLiteral("Removing one visible RN band should leave the remaining RN bands visible."));
    expectEqual(widget.testSpanCount(),
                2400,
                QStringLiteral("Removing a visible RN band should not delete cache lifetime data."));
}

void testCacheWidgetBoardLayoutDoesNotOverlap()
{
    CHIron::Gui::CacheWidget widget;
    showCacheForTest(widget);
    widget.resize(520, 260);
    widget.testInjectSyntheticSpans(2, 12);
    expect(widget.testAddVisibleBand(0),
           QStringLiteral("No-overlap fixture should expose one visible RN band."));
    QApplication::processEvents();

    const QVariantMap layout = widget.testLayoutState();
    const QRect header = layout.value(QStringLiteral("headerRect")).toRect();
    const QRect topRuler = layout.value(QStringLiteral("topRulerRect")).toRect();
    const QRect rail = layout.value(QStringLiteral("railRect")).toRect();
    const QRect plot = layout.value(QStringLiteral("plotRect")).toRect();
    const QRect bottomRuler = layout.value(QStringLiteral("bottomRulerRect")).toRect();
    const QRect horizontalScroll = layout.value(QStringLiteral("horizontalScrollRect")).toRect();
    const QRect verticalScroll = layout.value(QStringLiteral("verticalScrollRect")).toRect();
    const QRect addButton = layout.value(QStringLiteral("addBandButtonRect")).toRect();
    const QRect bandHeaderRail = layout.value(QStringLiteral("band0HeaderRailRect")).toRect();
    const QRect titleText = layout.value(QStringLiteral("band0TitleTextRect")).toRect();
    const QRect deleteButton = layout.value(QStringLiteral("band0DeleteButtonRect")).toRect();
    const QRect addressRail = layout.value(QStringLiteral("band0AddressRailRect")).toRect();
    const QRect addressPlot = layout.value(QStringLiteral("band0AddressPlotRect")).toRect();
    const QRect firstAddressRail = layout.value(QStringLiteral("band0FirstAddressRailRect")).toRect();
    const QRect firstAddressPlot = layout.value(QStringLiteral("band0FirstAddressPlotRect")).toRect();

    expect(header.isValid() && topRuler.isValid() && rail.isValid() && plot.isValid(),
           QStringLiteral("Cache board layout should expose valid fixed regions."));
    expect(header.contains(addButton),
           QStringLiteral("Cache + RN button should stay inside the fixed header."));
    expect(!rail.intersects(plot),
           QStringLiteral("Cache address rail should not overlap the plot area."));
    expect(!plot.intersects(verticalScroll),
           QStringLiteral("Cache vertical scrollbar should not overlap the plot area."));
    expect(plot.bottom() < bottomRuler.top(),
           QStringLiteral("Cache bottom timestamp ruler should be below the plot."));
    expect(bottomRuler.bottom() < horizontalScroll.top(),
           QStringLiteral("Cache horizontal scrollbar should be below the timestamp ruler."));
    expect(bandHeaderRail.contains(titleText),
           QStringLiteral("Cache RN title text should stay inside the RN header."));
    expect(bandHeaderRail.contains(deleteButton),
           QStringLiteral("Cache RN delete button should stay inside the RN header."));
    expect(!titleText.intersects(deleteButton),
           QStringLiteral("Cache RN title text should not overlap the delete button."));
    expect(addressRail.top() > bandHeaderRail.bottom(),
           QStringLiteral("Cache address labels should be below the RN header."));
    expect(addressPlot.top() > layout.value(QStringLiteral("band0HeaderPlotRect")).toRect().bottom(),
           QStringLiteral("Cache address-row plot should be below the RN plot header."));
    expect(firstAddressRail.isValid() && firstAddressPlot.isValid(),
           QStringLiteral("Readable Cache row mode should expose first address row rectangles."));
    expect(!firstAddressRail.intersects(firstAddressPlot),
           QStringLiteral("Cache address label rows should not overlap plotted lifetime rows."));

    for (int spanIndex = 0; spanIndex < widget.testSpanCount(); ++spanIndex) {
        const QRect spanRect = widget.testSpanVisualRect(spanIndex);
        if (spanRect.isEmpty()) {
            continue;
        }
        expect(!spanRect.intersects(rail),
               QStringLiteral("Cache lifetime bars should never overlap the left address rail."));
        expect(plot.adjusted(-2, -2, 2, 2).intersects(spanRect),
               QStringLiteral("Cache lifetime bars should be anchored in the plot area."));
    }
}

void testCacheWidgetLeftDragGesturesAndRangeZoom()
{
    CHIron::Gui::CacheWidget widget;
    showCacheForTest(widget);
    widget.testInjectSyntheticSpans(2, 120);
    widget.testAddVisibleBand(0);
    widget.testAddVisibleBand(1);
    QApplication::processEvents();

    const QRect plot = widget.testPlotRect();
    const QPoint start(plot.left() + plot.width() / 5, plot.center().y());
    const QPoint end(plot.left() + plot.width() * 2 / 3, plot.center().y() + 2);
    const auto before = widget.testVisibleTimestampRange();
    sendCacheMouse(widget,
                   QEvent::MouseButtonPress,
                   start,
                   Qt::LeftButton,
                   Qt::LeftButton,
                   Qt::NoModifier);
    sendCacheMouse(widget,
                   QEvent::MouseMove,
                   end,
                   Qt::NoButton,
                   Qt::LeftButton,
                   Qt::NoModifier);
    expect(widget.testGestureOverlayVisible(),
           QStringLiteral("Cache left-drag should show a gesture overlay while selecting a range."));
    expectEqual(widget.testDragModeName(),
                QStringLiteral("rangeZoom"),
                QStringLiteral("Mostly horizontal Cache left-drag should enter range-zoom mode."));
    sendCacheMouse(widget,
                   QEvent::MouseButtonRelease,
                   end,
                   Qt::LeftButton,
                   Qt::NoButton,
                   Qt::NoModifier);
    const auto after = widget.testVisibleTimestampRange();
    expect(after.second - after.first < before.second - before.first,
           QStringLiteral("Cache range zoom should narrow the visible timestamp range."));

    sendCacheKey(widget, Qt::Key_F);
    const double zoomBeforeDiagonal = widget.testHorizontalZoom();
    const QPoint diagonalEnd(start.x() + 44, start.y() + 36);
    sendCacheMouse(widget,
                   QEvent::MouseButtonPress,
                   start,
                   Qt::LeftButton,
                   Qt::LeftButton,
                   Qt::NoModifier);
    sendCacheMouse(widget,
                   QEvent::MouseMove,
                   diagonalEnd,
                   Qt::NoButton,
                   Qt::LeftButton,
                   Qt::NoModifier);
    expectEqual(widget.testDragModeName(),
                QStringLiteral("zoomIn2x"),
                QStringLiteral("Cache diagonal down-right drag should select 2x zoom-in."));
    sendCacheMouse(widget,
                   QEvent::MouseButtonRelease,
                   diagonalEnd,
                   Qt::LeftButton,
                   Qt::NoButton,
                   Qt::NoModifier);
    expectNear(widget.testHorizontalZoom(),
               zoomBeforeDiagonal * 2.0,
               0.05,
               QStringLiteral("Cache diagonal zoom-in gesture should double horizontal zoom."));

    const double verticalBandBefore = widget.testVerticalBandScale();
    const double verticalAddressBefore = widget.testVerticalAddressZoom();
    QWheelEvent wheel(QPointF(plot.center()),
                      QPointF(widget.mapToGlobal(plot.center())),
                      QPoint(),
                      QPoint(0, 120),
                      Qt::NoButton,
                      Qt::ControlModifier | Qt::ShiftModifier,
                      Qt::NoScrollPhase,
                      false);
    QApplication::sendEvent(&widget, &wheel);
    QApplication::processEvents();
    expect(widget.testVerticalBandScale() > verticalBandBefore,
           QStringLiteral("Ctrl+Shift+wheel should increase Cache vertical band scale."));
    expect(widget.testVerticalAddressZoom() > verticalAddressBefore,
           QStringLiteral("Ctrl+Shift+wheel should increase Cache address zoom."));
}

std::vector<CHIron::Gui::FlitRecord> buildLatencyWidgetTestRows()
{
    using CHIron::Gui::FlitChannel;
    using CHIron::Gui::FlitDirection;
    using CHIron::Gui::FlitRecord;

    auto makeRecord = [](const qint64 timestamp,
                         const FlitChannel channel,
                         const QString& opcode,
                         const QString& dataId,
                         const int ordinal) {
        FlitRecord record;
        record.timestamp = timestamp;
        record.channel = channel;
        record.direction = channel == FlitChannel::Req ? FlitDirection::Tx : FlitDirection::Rx;
        record.opcode = opcode;
        record.dataId = dataId;
        record.transactionGroupKey = QStringLiteral("xaction|%1").arg(ordinal);
        return record;
    };

    return {
        makeRecord(100, FlitChannel::Req, QStringLiteral("ReadNoSnp"), QString(), 1),
        makeRecord(110, FlitChannel::Rsp, QStringLiteral("CompDBIDResp"), QString(), 1),
        makeRecord(120, FlitChannel::Dat, QStringLiteral("CompData"), QStringLiteral("0"), 1),
        makeRecord(126, FlitChannel::Dat, QStringLiteral("CompData"), QStringLiteral("2"), 1),
        makeRecord(200, FlitChannel::Req, QStringLiteral("ReadNoSnp"), QString(), 2),
        makeRecord(215, FlitChannel::Dat, QStringLiteral("DataSepResp"), QStringLiteral("0"), 2),
        makeRecord(222, FlitChannel::Dat, QStringLiteral("DataSepResp"), QStringLiteral("2"), 2),
    };
}

struct LatencyRecordSpec {
    qint64 timestamp = 0;
    CHIron::Gui::FlitChannel channel = CHIron::Gui::FlitChannel::Req;
    QString opcode;
    int ordinal = 0;
};

CHIron::Gui::FlitRecord makeLatencyRecord(const qint64 timestamp,
                                          const CHIron::Gui::FlitChannel channel,
                                          const QString& opcode,
                                          const int ordinal)
{
    CHIron::Gui::FlitRecord record;
    record.timestamp = timestamp;
    record.channel = channel;
    record.direction = channel == CHIron::Gui::FlitChannel::Req
        ? CHIron::Gui::FlitDirection::Tx
        : CHIron::Gui::FlitDirection::Rx;
    record.opcode = opcode;
    record.transactionGroupKey = QStringLiteral("xaction|%1").arg(ordinal);
    return record;
}

std::vector<CHIron::Gui::FlitRecord> buildLatencyRows(std::initializer_list<LatencyRecordSpec> specs)
{
    std::vector<CHIron::Gui::FlitRecord> rows;
    rows.reserve(specs.size());
    for (const LatencyRecordSpec& spec : specs) {
        rows.push_back(makeLatencyRecord(spec.timestamp, spec.channel, spec.opcode, spec.ordinal));
    }
    return rows;
}

const CHIron::Gui::LatencyBucket* findAnalysisLeaf(const CHIron::Gui::LatencyAnalysisResult& result,
                                                   const QString& type,
                                                   const QString& request,
                                                   const QString& subsequent)
{
    const auto typeIt = result.typeBuckets.find(type);
    if (typeIt == result.typeBuckets.end()) {
        return nullptr;
    }
    const auto requestIt = typeIt->second.requestOpcodeBuckets.find(request);
    if (requestIt == typeIt->second.requestOpcodeBuckets.end()) {
        return nullptr;
    }
    const auto leafIt = requestIt->second.subsequentBuckets.find(subsequent);
    return leafIt == requestIt->second.subsequentBuckets.end() ? nullptr : &leafIt->second;
}

const CHIron::Gui::LatencyDiffRow* findDiffRow(const CHIron::Gui::LatencyDiffReport& report,
                                               const QStringList& path)
{
    const auto found = std::find_if(report.rows.begin(), report.rows.end(), [&](const CHIron::Gui::LatencyDiffRow& row) {
        return row.path == path;
    });
    return found == report.rows.end() ? nullptr : &*found;
}

const CHIron::Gui::LatencyDiffMetric* findDiffMetric(const CHIron::Gui::LatencyDiffRow& row,
                                                     const CHIron::Gui::LatencyDiffMetricKind kind)
{
    const auto found = std::find_if(row.metrics.begin(), row.metrics.end(), [&](const CHIron::Gui::LatencyDiffMetric& metric) {
        return metric.kind == kind;
    });
    return found == row.metrics.end() ? nullptr : &*found;
}

CHIron::Gui::LatencyDiffReport buildSimpleLatencyDiffReport()
{
    using CHIron::Gui::FlitChannel;

    std::vector<CHIron::Gui::FlitRecord> rowsA = buildLatencyRows({
        {100, FlitChannel::Req, QStringLiteral("ReadNoSnp"), 1},
        {110, FlitChannel::Rsp, QStringLiteral("Comp"), 1},
    });
    std::vector<CHIron::Gui::FlitRecord> rowsB = buildLatencyRows({
        {100, FlitChannel::Req, QStringLiteral("ReadNoSnp"), 1},
        {120, FlitChannel::Rsp, QStringLiteral("Comp"), 1},
    });

    CHIron::Gui::LatencyDiffOptions options;
    options.sessionALabel = QStringLiteral("A");
    options.sessionBLabel = QStringLiteral("B");
    return CHIron::Gui::BuildLatencyDiffReport(
        CHIron::Gui::AnalyzeLatencyRows(rowsA, {}, QStringLiteral("A")),
        CHIron::Gui::AnalyzeLatencyRows(rowsB, {}, QStringLiteral("B")),
        options);
}

void testLatencyWidgetBuildsExpandableOpcodeBeatBuckets()
{
    CHIron::Gui::LatencyWidget widget;
    widget.setRows(buildLatencyWidgetTestRows());
    widget.buildView();

    expectEqual(widget.testColumnCount(),
                2,
                QStringLiteral("Latency widget should display only classification and box-plot columns."));
    expect(widget.testColumnsAreUserResizable(),
           QStringLiteral("Latency widget should keep classification resizable while stretching the plot to the right edge."));
    expectEqual(widget.testTopLevelCount(),
                1,
                QStringLiteral("Latency widget should group test rows under one inferred Xaction type."));
    expectEqual(widget.testItemText({0}, 0),
                QStringLiteral("Unknown Xaction"),
                QStringLiteral("Fallback rows without persisted index details should use an unknown Xaction type."));
    expectEqual(widget.testChildCount({0}),
                1,
                QStringLiteral("Latency widget should classify first by xaction type and then by request opcode."));
    expectEqual(widget.testItemText({0, 0}, 0),
                QStringLiteral("REQ ReadNoSnp"),
                QStringLiteral("Latency widget should use the first REQ/SNP opcode as the second-level bucket."));
    expectEqual(widget.testChildCount({0, 0}),
                5,
                QStringLiteral("Latency widget should split subsequent opcodes and data beat roles."));

    QStringList leafNames;
    for (int index = 0; index < widget.testChildCount({0, 0}); ++index) {
        leafNames.append(widget.testItemText({0, 0, index}, 0));
    }
    expect(leafNames.contains(QStringLiteral("RSP CompDBIDResp")),
           QStringLiteral("Response opcode latency should be shown as a subsequent flit bucket."));
    expect(leafNames.contains(QStringLiteral("CompData first beat")),
           QStringLiteral("CompData first beat should be shown as its own latency bucket."));
    expect(leafNames.contains(QStringLiteral("CompData following beats")),
           QStringLiteral("CompData following beats should be shown as its own latency bucket."));
    expect(leafNames.contains(QStringLiteral("DataSepResp first beat")),
           QStringLiteral("DataSepResp first beat should be shown as its own latency bucket."));
    expect(leafNames.contains(QStringLiteral("DataSepResp following beats")),
           QStringLiteral("DataSepResp following beats should be shown as its own latency bucket."));
    expect(widget.testItemText({0, 0}, 1).isEmpty(),
           QStringLiteral("Latency numeric values should not be printed directly in the box-plot column."));
    expect(widget.testItemToolTip({0, 0}, 1).isEmpty(),
           QStringLiteral("Latency summary values should be shown in the selected plot region, not a tooltip."));
}

void expectLatencyStat(const QVariantMap& stats,
                       const QString& key,
                       const qlonglong expected,
                       const QString& message)
{
    expect(stats.contains(key),
           QStringLiteral("%1 (missing key %2)").arg(message, key));
    const qlonglong actual = stats.value(key).toLongLong();
    if (actual != expected) {
        fail(QStringLiteral("%1 (actual=%2, expected=%3)")
                 .arg(message)
                 .arg(actual)
                 .arg(expected));
    }
}

void testLatencyWidgetRowStatisticSummaries()
{
    CHIron::Gui::LatencyWidget widget;
    widget.setRows(buildLatencyWidgetTestRows());
    widget.buildView();

    const QVariantMap typeStats = widget.testItemStats({0});
    expectLatencyStat(typeStats, QStringLiteral("count"), 2, QStringLiteral("Type row should count completed transactions."));
    expectLatencyStat(typeStats, QStringLiteral("min"), 22, QStringLiteral("Type row minimum completion latency should be correct."));
    expectLatencyStat(typeStats, QStringLiteral("q1"), 23, QStringLiteral("Type row Q1 completion latency should be correct."));
    expectLatencyStat(typeStats, QStringLiteral("median"), 24, QStringLiteral("Type row median completion latency should be correct."));
    expectLatencyStat(typeStats, QStringLiteral("q3"), 25, QStringLiteral("Type row Q3 completion latency should be correct."));
    expectLatencyStat(typeStats, QStringLiteral("max"), 26, QStringLiteral("Type row maximum latency should be correct."));
    expectLatencyStat(typeStats, QStringLiteral("mean"), 24, QStringLiteral("Type row mean completion latency should be rounded correctly."));
    expectLatencyStat(typeStats, QStringLiteral("lowerWhisker"), 22, QStringLiteral("Type row lower whisker should be correct."));
    expectLatencyStat(typeStats, QStringLiteral("upperWhisker"), 26, QStringLiteral("Type row upper whisker should be correct."));
    expectLatencyStat(typeStats, QStringLiteral("outlierCount"), 0, QStringLiteral("Type row should have no outliers."));

    const QStringList typeSummary = widget.testItemSummaryLines({0});
    expect(typeSummary.contains(QStringLiteral("Count 2")),
           QStringLiteral("Type summary should display the completed transaction count."));
    expect(typeSummary.contains(QStringLiteral("Min 22")),
           QStringLiteral("Type summary should display the minimum latency."));
    expect(typeSummary.contains(QStringLiteral("Q1 23")),
           QStringLiteral("Type summary should display Q1."));
    expect(typeSummary.contains(QStringLiteral("Median 24")),
           QStringLiteral("Type summary should display the median."));
    expect(typeSummary.contains(QStringLiteral("Q3 25")),
           QStringLiteral("Type summary should display Q3."));
    expect(typeSummary.contains(QStringLiteral("Max 26")),
           QStringLiteral("Type summary should display the maximum latency."));
    expect(typeSummary.contains(QStringLiteral("Mean 24")),
           QStringLiteral("Type summary should display the rounded mean."));
    expect(typeSummary.contains(QStringLiteral("Outliers 0")),
           QStringLiteral("Type summary should display the outlier count."));

    const QVariantMap requestStats = widget.testItemStats({0, 0});
    expectLatencyStat(requestStats, QStringLiteral("count"), 2, QStringLiteral("Request row should count completed transactions for that request opcode."));
    expectLatencyStat(requestStats, QStringLiteral("median"), 24, QStringLiteral("Request row median should use request completion latencies."));

    for (int index = 0; index < widget.testChildCount({0, 0}); ++index) {
        const QString name = widget.testItemText({0, 0, index}, 0);
        const QVariantMap leafStats = widget.testItemStats({0, 0, index});
        if (name == QStringLiteral("RSP CompDBIDResp")) {
            expectLatencyStat(leafStats, QStringLiteral("count"), 1, QStringLiteral("RSP leaf should have one sample."));
            expectLatencyStat(leafStats, QStringLiteral("min"), 10, QStringLiteral("RSP leaf latency should be 10."));
        } else if (name == QStringLiteral("CompData first beat")) {
            expectLatencyStat(leafStats, QStringLiteral("count"), 1, QStringLiteral("CompData first-beat leaf should have one sample."));
            expectLatencyStat(leafStats, QStringLiteral("min"), 20, QStringLiteral("CompData first-beat latency should be 20."));
        } else if (name == QStringLiteral("CompData following beats")) {
            expectLatencyStat(leafStats, QStringLiteral("count"), 1, QStringLiteral("CompData following-beat leaf should have one sample."));
            expectLatencyStat(leafStats, QStringLiteral("min"), 26, QStringLiteral("CompData following-beat latency should be 26."));
        } else if (name == QStringLiteral("DataSepResp first beat")) {
            expectLatencyStat(leafStats, QStringLiteral("count"), 1, QStringLiteral("DataSepResp first-beat leaf should have one sample."));
            expectLatencyStat(leafStats, QStringLiteral("min"), 15, QStringLiteral("DataSepResp first-beat latency should be 15."));
        } else if (name == QStringLiteral("DataSepResp following beats")) {
            expectLatencyStat(leafStats, QStringLiteral("count"), 1, QStringLiteral("DataSepResp following-beat leaf should have one sample."));
            expectLatencyStat(leafStats, QStringLiteral("min"), 22, QStringLiteral("DataSepResp following-beat latency should be 22."));
        }
    }

    widget.testSelectItem({0});
    expectEqual(widget.testSummaryTitle(),
                QStringLiteral("Unknown Xaction"),
                QStringLiteral("Selecting a type row should update the summary title."));
    expectEqual(widget.testSummaryBody(),
                typeSummary.join(QLatin1Char('\n')),
                QStringLiteral("Selected summary panel should display the same computed type statistics."));
}

void testLatencyAnalysisMatchesLatencyWidgetRows()
{
    const CHIron::Gui::LatencyAnalysisResult result =
        CHIron::Gui::AnalyzeLatencyRows(buildLatencyWidgetTestRows(), {}, QStringLiteral("rows"));

    expect(!result.failed, result.errorText);
    expectEqual(result.typeBuckets.size(),
                static_cast<std::size_t>(1),
                QStringLiteral("Shared latency analysis should create the same top-level xaction bucket."));
    const auto typeIt = result.typeBuckets.find(QStringLiteral("Unknown Xaction"));
    expect(typeIt != result.typeBuckets.end(),
           QStringLiteral("Shared latency analysis should preserve the Latency widget fallback xaction type."));

    const CHIron::Gui::LatencyStats typeStats =
        CHIron::Gui::CalculateLatencyStats(typeIt->second.samples);
    expectEqual(static_cast<std::size_t>(typeStats.count),
                static_cast<std::size_t>(2),
                QStringLiteral("Shared latency analysis should count completion samples like Latency widget."));
    expectNear(typeStats.min, 22.0, 0.0001, QStringLiteral("Shared latency analysis min should match Latency widget."));
    expectNear(typeStats.q1, 23.0, 0.0001, QStringLiteral("Shared latency analysis Q1 should match Latency widget."));
    expectNear(typeStats.median, 24.0, 0.0001, QStringLiteral("Shared latency analysis median should match Latency widget."));
    expectNear(typeStats.q3, 25.0, 0.0001, QStringLiteral("Shared latency analysis Q3 should match Latency widget."));
    expectNear(typeStats.max, 26.0, 0.0001, QStringLiteral("Shared latency analysis max should match Latency widget."));
    expectNear(typeStats.mean, 24.0, 0.0001, QStringLiteral("Shared latency analysis mean should match Latency widget."));
    expectNear(typeStats.lowerWhisker,
               22.0,
               0.0001,
               QStringLiteral("Shared latency analysis lower whisker should match Latency widget."));
    expectNear(typeStats.upperWhisker,
               26.0,
               0.0001,
               QStringLiteral("Shared latency analysis upper whisker should match Latency widget."));
    expectEqual(static_cast<std::size_t>(typeStats.outlierCount),
                static_cast<std::size_t>(0),
                QStringLiteral("Shared latency analysis outlier count should match Latency widget."));

    const CHIron::Gui::LatencyBucket* responseLeaf =
        findAnalysisLeaf(result,
                         QStringLiteral("Unknown Xaction"),
                         QStringLiteral("REQ ReadNoSnp"),
                         QStringLiteral("RSP CompDBIDResp"));
    expect(responseLeaf != nullptr,
           QStringLiteral("Shared latency analysis should preserve the response leaf bucket."));
    const CHIron::Gui::LatencyStats responseStats =
        CHIron::Gui::CalculateLatencyStats(responseLeaf->samples);
    expectEqual(static_cast<std::size_t>(responseStats.count),
                static_cast<std::size_t>(1),
                QStringLiteral("Shared latency response leaf should have one sample."));
    expectNear(responseStats.min,
               10.0,
               0.0001,
               QStringLiteral("Shared latency response leaf latency should match Latency widget."));
    expect(responseLeaf->samples.front().rawStartTimestamp == 100,
           QStringLiteral("Shared latency samples should retain the raw transaction start timestamp."));
    expect(responseLeaf->samples.front().rawEndTimestamp == 110,
           QStringLiteral("Shared latency samples should retain the raw subsequent flit timestamp."));
    expectEqual(responseLeaf->samples.front().logicalRow,
                1,
                QStringLiteral("Shared latency samples should retain the related logical row."));
}

void testLatencyDiffReportMatchedUnionAndZeroBaseline()
{
    using CHIron::Gui::FlitChannel;

    std::vector<CHIron::Gui::FlitRecord> rowsA = buildLatencyRows({
        {100, FlitChannel::Req, QStringLiteral("ReadNoSnp"), 1},
        {110, FlitChannel::Rsp, QStringLiteral("Comp"), 1},
        {200, FlitChannel::Req, QStringLiteral("ReadNoSnp"), 2},
        {220, FlitChannel::Rsp, QStringLiteral("Comp"), 2},
        {300, FlitChannel::Req, QStringLiteral("WriteNoSnp"), 3},
        {315, FlitChannel::Rsp, QStringLiteral("Resp"), 3},
        {400, FlitChannel::Req, QStringLiteral("ZeroBase"), 4},
        {400, FlitChannel::Rsp, QStringLiteral("Done"), 4},
    });
    std::vector<CHIron::Gui::FlitRecord> rowsB = buildLatencyRows({
        {100, FlitChannel::Req, QStringLiteral("ReadNoSnp"), 1},
        {130, FlitChannel::Rsp, QStringLiteral("Comp"), 1},
        {200, FlitChannel::Req, QStringLiteral("ReadNoSnp"), 2},
        {240, FlitChannel::Rsp, QStringLiteral("Comp"), 2},
        {500, FlitChannel::Req, QStringLiteral("ReadUnique"), 3},
        {520, FlitChannel::Rsp, QStringLiteral("Comp"), 3},
        {600, FlitChannel::Req, QStringLiteral("ZeroBase"), 4},
        {610, FlitChannel::Rsp, QStringLiteral("Done"), 4},
    });

    CHIron::Gui::LatencyDiffOptions options;
    options.sessionALabel = QStringLiteral("A");
    options.sessionBLabel = QStringLiteral("B");
    const CHIron::Gui::LatencyDiffReport report =
        CHIron::Gui::BuildLatencyDiffReport(
            CHIron::Gui::AnalyzeLatencyRows(rowsA, {}, QStringLiteral("A")),
            CHIron::Gui::AnalyzeLatencyRows(rowsB, {}, QStringLiteral("B")),
            options);

    expect(!report.failed, report.errorText);
    const CHIron::Gui::LatencyDiffRow* matched =
        findDiffRow(report,
                    {QStringLiteral("Unknown Xaction"),
                     QStringLiteral("REQ ReadNoSnp"),
                     QStringLiteral("RSP Comp")});
    expect(matched != nullptr,
           QStringLiteral("Latency Diff should include matched buckets."));
    const CHIron::Gui::LatencyDiffMetric* matchedMean =
        findDiffMetric(*matched, CHIron::Gui::LatencyDiffMetricKind::Mean);
    expect(matchedMean != nullptr, QStringLiteral("Latency Diff should include the Mean metric."));
    expectNear(matchedMean->aValue, 15.0, 0.0001, QStringLiteral("Matched Mean A value should be correct."));
    expectNear(matchedMean->bValue, 35.0, 0.0001, QStringLiteral("Matched Mean B value should be correct."));
    expectNear(matchedMean->delta, 20.0, 0.0001, QStringLiteral("Matched Mean delta should be correct."));
    expectNear(matchedMean->percent, 20.0 / 15.0, 0.0001, QStringLiteral("Matched Mean percent diff should be (B-A)/A."));

    const CHIron::Gui::LatencyDiffRow* aOnly =
        findDiffRow(report,
                    {QStringLiteral("Unknown Xaction"),
                     QStringLiteral("REQ WriteNoSnp"),
                     QStringLiteral("RSP Resp")});
    expect(aOnly && aOnly->presentInA && !aOnly->presentInB,
           QStringLiteral("Latency Diff should include A-only buckets."));
    const CHIron::Gui::LatencyDiffRow* bOnly =
        findDiffRow(report,
                    {QStringLiteral("Unknown Xaction"),
                     QStringLiteral("REQ ReadUnique"),
                     QStringLiteral("RSP Comp")});
    expect(bOnly && !bOnly->presentInA && bOnly->presentInB,
           QStringLiteral("Latency Diff should include B-only buckets."));

    const CHIron::Gui::LatencyDiffRow* zeroBase =
        findDiffRow(report,
                    {QStringLiteral("Unknown Xaction"),
                     QStringLiteral("REQ ZeroBase"),
                     QStringLiteral("RSP Done")});
    expect(zeroBase != nullptr,
           QStringLiteral("Latency Diff should include zero-baseline buckets."));
    const CHIron::Gui::LatencyDiffMetric* zeroMin =
        findDiffMetric(*zeroBase, CHIron::Gui::LatencyDiffMetricKind::Min);
    expect(zeroMin != nullptr, QStringLiteral("Zero baseline row should include Min metric."));
    expectNear(zeroMin->aValue, 0.0, 0.0001, QStringLiteral("Zero baseline A value should be zero."));
    expectNear(zeroMin->bValue, 10.0, 0.0001, QStringLiteral("Zero baseline B value should be nonzero."));
    expect(!zeroMin->percentComparable,
           QStringLiteral("Zero baseline with nonzero comparison should not be percentage-comparable."));
    expectEqual(static_cast<int>(zeroBase->metrics.size()),
                static_cast<int>(CHIron::Gui::LatencyDiffMetricKinds().size()),
                QStringLiteral("Latency Diff should emit all statistic fields for every row."));
}

void testLatencyAnalysisRangeUsesScaledStartTimestamp()
{
    using CHIron::Gui::FlitChannel;

    const std::vector<CHIron::Gui::FlitRecord> rows = buildLatencyRows({
        {100, FlitChannel::Req, QStringLiteral("ReadNoSnp"), 1},
        {110, FlitChannel::Rsp, QStringLiteral("Comp"), 1},
        {200, FlitChannel::Req, QStringLiteral("ReadNoSnp"), 2},
        {210, FlitChannel::Rsp, QStringLiteral("Comp"), 2},
    });

    CHIron::Gui::LatencyAnalysisOptions relativeOptions;
    relativeOptions.anchorMode = CHIron::Gui::LatencyTimeAnchorMode::RelativeSessionStart;
    relativeOptions.timeScale = 0.1;
    relativeOptions.hasRange = true;
    relativeOptions.rangeStart = 0.0;
    relativeOptions.rangeEnd = 5.0;
    const CHIron::Gui::LatencyAnalysisResult relative =
        CHIron::Gui::AnalyzeLatencyRows(rows, relativeOptions, QStringLiteral("relative"));
    const auto relativeType = relative.typeBuckets.find(QStringLiteral("Unknown Xaction"));
    expect(relativeType != relative.typeBuckets.end(),
           QStringLiteral("Relative range analysis should retain the first transaction bucket."));
    expectEqual(relativeType->second.samples.size(),
                static_cast<std::size_t>(1),
                QStringLiteral("Relative scaled range should include only the transaction whose start falls inside 0..5."));
    expectNear(relativeType->second.samples.front().latency,
               1.0,
               0.0001,
               QStringLiteral("Latency should use scaled timestamps after relative anchoring."));

    CHIron::Gui::LatencyAnalysisOptions absoluteOptions;
    absoluteOptions.anchorMode = CHIron::Gui::LatencyTimeAnchorMode::AbsoluteTimestamp;
    absoluteOptions.absoluteAnchor = 0;
    absoluteOptions.timeScale = 1.0;
    absoluteOptions.hasRange = true;
    absoluteOptions.rangeStart = 190.0;
    absoluteOptions.rangeEnd = 205.0;
    const CHIron::Gui::LatencyAnalysisResult absolute =
        CHIron::Gui::AnalyzeLatencyRows(rows, absoluteOptions, QStringLiteral("absolute"));
    const auto absoluteType = absolute.typeBuckets.find(QStringLiteral("Unknown Xaction"));
    expect(absoluteType != absolute.typeBuckets.end(),
           QStringLiteral("Absolute range analysis should retain the second transaction bucket."));
    expectEqual(absoluteType->second.samples.size(),
                static_cast<std::size_t>(1),
                QStringLiteral("Absolute range should use raw-start timestamps against the absolute anchor."));
    expect(absoluteType->second.samples.front().rawStartTimestamp == 200,
           QStringLiteral("Absolute range should include based on transaction start timestamp."));

    CHIron::Gui::LatencyDiffOptions diffOptions;
    diffOptions.sessionALabel = QStringLiteral("A");
    diffOptions.sessionBLabel = QStringLiteral("B");
    diffOptions.rangeMode = CHIron::Gui::LatencyDiffRangeMode::SeparateSessionRanges;
    diffOptions.sessionA = relativeOptions;
    diffOptions.sessionB = absoluteOptions;
    const CHIron::Gui::LatencyDiffReport report =
        CHIron::Gui::BuildLatencyDiffReport(
            CHIron::Gui::AnalyzeLatencyRows(rows, relativeOptions, QStringLiteral("A")),
            CHIron::Gui::AnalyzeLatencyRows(rows, absoluteOptions, QStringLiteral("B")),
            diffOptions);
    const CHIron::Gui::LatencyDiffRow* row =
        findDiffRow(report,
                    {QStringLiteral("Unknown Xaction"),
                     QStringLiteral("REQ ReadNoSnp"),
                     QStringLiteral("RSP Comp")});
    expect(row != nullptr && row->presentInA && row->presentInB,
           QStringLiteral("Separate range Latency Diff should compare independently filtered sessions."));
    const CHIron::Gui::LatencyDiffMetric* mean =
        findDiffMetric(*row, CHIron::Gui::LatencyDiffMetricKind::Mean);
    expect(mean != nullptr, QStringLiteral("Separate range Latency Diff should include Mean."));
    expectNear(mean->aValue, 1.0, 0.0001, QStringLiteral("Session A mean should use its scaled range."));
    expectNear(mean->bValue, 10.0, 0.0001, QStringLiteral("Session B mean should use its absolute range."));
}

void testLatencyDiffWidgetBuildsDataBarsAndSortsPercentages()
{
    using CHIron::Gui::FlitChannel;

    CHIron::Gui::LatencyDiffWidget widget;
    widget.setSessions({
        CHIron::Gui::LatencyDiffSessionSource{
            .id = 1,
            .label = QStringLiteral("A"),
            .path = QString(),
            .traceSession = nullptr,
            .rows = buildLatencyRows({
                {100, FlitChannel::Req, QStringLiteral("ReadNoSnp"), 1},
                {110, FlitChannel::Rsp, QStringLiteral("Comp"), 1},
                {200, FlitChannel::Req, QStringLiteral("WriteNoSnp"), 2},
                {220, FlitChannel::Rsp, QStringLiteral("Resp"), 2},
            }),
        },
        CHIron::Gui::LatencyDiffSessionSource{
            .id = 2,
            .label = QStringLiteral("B"),
            .path = QString(),
            .traceSession = nullptr,
            .rows = buildLatencyRows({
                {100, FlitChannel::Req, QStringLiteral("ReadNoSnp"), 1},
                {130, FlitChannel::Rsp, QStringLiteral("Comp"), 1},
                {200, FlitChannel::Req, QStringLiteral("WriteNoSnp"), 2},
                {210, FlitChannel::Rsp, QStringLiteral("Resp"), 2},
            }),
        },
    });
    expect(widget.testBuildDefaultDiff(),
           QStringLiteral("Latency Diff widget should build from row-backed session sources."));
    showLatencyDiffForTest(widget);

    expectEqual(widget.testTopLevelCount(),
                1,
                QStringLiteral("Latency Diff widget should build a hierarchical xaction root."));
    expectEqual(widget.testItemText({0}, 0),
                QStringLiteral("Unknown Xaction"),
                QStringLiteral("Latency Diff widget should use the shared xaction grouping labels."));
    expect(widget.testChildCount({0}) >= 2,
           QStringLiteral("Latency Diff widget should expose request buckets below the xaction root."));

    auto* tree = widget.findChild<QTreeWidget*>();
    expect(tree != nullptr, QStringLiteral("Latency Diff widget should own a result tree."));
    tree->sortItems(4, Qt::DescendingOrder);
    QApplication::processEvents();
    bool foundComparablePercent = false;
    bool foundNegativePercent = false;
    for (int topIndex = 0; topIndex < tree->topLevelItemCount(); ++topIndex) {
        QTreeWidgetItem* top = tree->topLevelItem(topIndex);
        for (int requestIndex = 0; requestIndex < top->childCount(); ++requestIndex) {
            QTreeWidgetItem* request = top->child(requestIndex);
            for (int leafIndex = 0; leafIndex < request->childCount(); ++leafIndex) {
                QTreeWidgetItem* leaf = request->child(leafIndex);
                for (int metricIndex = 0; metricIndex < leaf->childCount(); ++metricIndex) {
                    QTreeWidgetItem* metric = leaf->child(metricIndex);
                    const QVariant comparable = metric->data(4, Qt::UserRole + 2);
                    const QVariant percent = metric->data(4, Qt::UserRole + 1);
                    if (comparable.toBool() && percent.isValid()) {
                        foundComparablePercent = true;
                        foundNegativePercent = foundNegativePercent || percent.toDouble() < 0.0;
                    }
                }
            }
        }
    }
    expect(foundComparablePercent,
           QStringLiteral("Latency Diff widget should attach comparable percent data for data-bar rendering."));
    expect(foundNegativePercent,
           QStringLiteral("Latency Diff widget should attach negative percent data for faster comparison metrics."));

    QImage image(widget.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    widget.render(&painter);
    painter.end();

    int barPixels = 0;
    for (int y = 0; y < image.height(); y += 2) {
        for (int x = 0; x < image.width(); x += 2) {
            const QColor pixel = QColor::fromRgba(image.pixel(x, y));
            const bool positiveBar = pixel.red() > 230 && pixel.green() > 150 && pixel.green() < 220 && pixel.blue() < 210;
            const bool negativeBar = pixel.green() > 200 && pixel.red() < 220 && pixel.blue() < 230;
            if (positiveBar || negativeBar) {
                ++barPixels;
            }
        }
    }
    expectGreater(barPixels,
                  4,
                  QStringLiteral("Latency Diff widget should render Excel-like data bars in percentage cells."));
}

void testLatencyDiffExportWritesWorkbookDataBars()
{
    const CHIron::Gui::LatencyDiffReport report = buildSimpleLatencyDiffReport();
    expect(!report.failed, report.errorText);

    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    const QString path = tempDir.filePath(QStringLiteral("latency-diff.xlsx"));
    QString errorText;
    expect(CHIron::Gui::ExportLatencyDiffReportXlsx(report, path, errorText),
           errorText.isEmpty()
               ? QStringLiteral("Latency Diff export should write a workbook.")
               : errorText);

    QFile file(path);
    expect(file.open(QIODevice::ReadOnly), QStringLiteral("Exported Latency Diff workbook should be readable."));
    const QByteArray data = file.readAll();
    expect(data.startsWith("PK"),
           QStringLiteral("Latency Diff export should be a ZIP-backed .xlsx package."));
    expect(data.contains("xl/workbook.xml"),
           QStringLiteral("Latency Diff export should include the workbook part."));
    expect(data.contains("xl/worksheets/sheet1.xml"),
           QStringLiteral("Latency Diff export should include the summary sheet part."));
    expect(data.contains("xl/worksheets/sheet2.xml"),
           QStringLiteral("Latency Diff export should include the Latency Diff sheet part."));
    expect(data.contains("Latency Diff"),
           QStringLiteral("Latency Diff export should name the report sheet."));
    expect(data.contains("conditionalFormatting"),
           QStringLiteral("Latency Diff export should include conditional formatting XML."));
    expect(data.contains("dataBar"),
           QStringLiteral("Latency Diff export should include real Excel data-bar rules."));
}

void testMainWindowLatencyDiffTracksOpenSessions()
{
    CHIron::Gui::MainWindow window;
    window.show();
    QApplication::processEvents();

    expect(window.testApplyTraceRows(buildLatencyRows({
                                      {100, CHIron::Gui::FlitChannel::Req, QStringLiteral("ReadNoSnp"), 1},
                                      {110, CHIron::Gui::FlitChannel::Rsp, QStringLiteral("Comp"), 1},
                                  }),
                                  QStringLiteral("A")),
           QStringLiteral("MainWindow should load the first row-backed Latency Diff source."));
    expect(window.testApplyTraceRows(buildLatencyRows({
                                      {100, CHIron::Gui::FlitChannel::Req, QStringLiteral("ReadNoSnp"), 1},
                                      {120, CHIron::Gui::FlitChannel::Rsp, QStringLiteral("Comp"), 1},
                                  }),
                                  QStringLiteral("B")),
           QStringLiteral("MainWindow should load the second row-backed Latency Diff source."));
    QApplication::processEvents();

    expectEqual(window.testLatencyDiffSessionCount(),
                2,
                QStringLiteral("MainWindow should expose all open sessions to the global Latency Diff widget."));
    expect(window.testBuildDefaultLatencyDiff(),
           QStringLiteral("MainWindow Latency Diff widget should build a default comparison from two open sessions."));
    expect(window.testLatencyDiffHasReport(),
           QStringLiteral("MainWindow Latency Diff widget should retain the built report."));
    window.testCloseActiveSession();
    QApplication::processEvents();
    expectEqual(window.testLatencyDiffSessionCount(),
                1,
                QStringLiteral("MainWindow should refresh Latency Diff sources after closing a session."));
}

std::vector<CHIron::Gui::FlitRecord> buildLatencyWidgetOutlierRows()
{
    using CHIron::Gui::FlitChannel;
    using CHIron::Gui::FlitDirection;
    using CHIron::Gui::FlitRecord;

    const std::array<qint64, 9> latencies{10, 11, 12, 13, 14, 15, 16, 17, 100};
    std::vector<FlitRecord> rows;
    rows.reserve(latencies.size() * 2U);
    for (std::size_t index = 0; index < latencies.size(); ++index) {
        const int ordinal = static_cast<int>(index + 1U);
        const qint64 start = static_cast<qint64>(index) * 1000;

        FlitRecord request;
        request.timestamp = start;
        request.channel = FlitChannel::Req;
        request.direction = FlitDirection::Tx;
        request.opcode = QStringLiteral("ReadNoSnp");
        request.transactionGroupKey = QStringLiteral("xaction|%1").arg(ordinal);
        rows.push_back(request);

        FlitRecord response;
        response.timestamp = start + latencies[index];
        response.channel = FlitChannel::Rsp;
        response.direction = FlitDirection::Rx;
        response.opcode = QStringLiteral("Comp");
        response.transactionGroupKey = QStringLiteral("xaction|%1").arg(ordinal);
        rows.push_back(response);
    }
    return rows;
}

std::vector<CHIron::Gui::FlitRecord> buildLatencyWidgetCongestedJitterRows()
{
    using CHIron::Gui::FlitChannel;
    using CHIron::Gui::FlitDirection;
    using CHIron::Gui::FlitRecord;

    std::vector<FlitRecord> rows;
    rows.reserve(720);
    for (int ordinal = 1; ordinal <= 360; ++ordinal) {
        const qint64 start = static_cast<qint64>(ordinal) * 100;

        FlitRecord request;
        request.timestamp = start;
        request.channel = FlitChannel::Req;
        request.direction = FlitDirection::Tx;
        request.opcode = QStringLiteral("ReadNoSnp");
        request.transactionGroupKey = QStringLiteral("xaction|%1").arg(ordinal);
        rows.push_back(request);

        FlitRecord response;
        response.timestamp = start + 20;
        response.channel = FlitChannel::Rsp;
        response.direction = FlitDirection::Rx;
        response.opcode = QStringLiteral("Comp");
        response.transactionGroupKey = request.transactionGroupKey;
        rows.push_back(response);
    }
    return rows;
}

void testLatencyWidgetOutlierStatisticSummaries()
{
    CHIron::Gui::LatencyWidget widget;
    widget.setRows(buildLatencyWidgetOutlierRows());
    widget.buildView();

    expectEqual(widget.testTopLevelCount(),
                1,
                QStringLiteral("Outlier test should create one xaction type row."));
    expectEqual(widget.testChildCount({0, 0}),
                1,
                QStringLiteral("Outlier test should create one subsequent response bucket."));

    const QVariantMap stats = widget.testItemStats({0, 0, 0});
    expectLatencyStat(stats, QStringLiteral("count"), 9, QStringLiteral("Outlier leaf should count all samples."));
    expectLatencyStat(stats, QStringLiteral("min"), 10, QStringLiteral("Outlier leaf minimum should be correct."));
    expectLatencyStat(stats, QStringLiteral("q1"), 12, QStringLiteral("Outlier leaf Q1 should use interpolated percentile."));
    expectLatencyStat(stats, QStringLiteral("median"), 14, QStringLiteral("Outlier leaf median should be correct."));
    expectLatencyStat(stats, QStringLiteral("q3"), 16, QStringLiteral("Outlier leaf Q3 should use interpolated percentile."));
    expectLatencyStat(stats, QStringLiteral("max"), 100, QStringLiteral("Outlier leaf maximum should include the outlier."));
    expectLatencyStat(stats, QStringLiteral("mean"), 23, QStringLiteral("Outlier leaf mean should be rounded correctly."));
    expectLatencyStat(stats, QStringLiteral("lowerWhisker"), 10, QStringLiteral("Outlier leaf lower whisker should be first non-outlier."));
    expectLatencyStat(stats, QStringLiteral("upperWhisker"), 17, QStringLiteral("Outlier leaf upper whisker should exclude right outlier."));
    expectLatencyStat(stats, QStringLiteral("outlierCount"), 1, QStringLiteral("Outlier leaf should count the right outlier."));
    const QVariantList outliers = stats.value(QStringLiteral("outliers")).toList();
    expectEqual(outliers.size(),
                1,
                QStringLiteral("Outlier leaf should retain the rendered outlier value."));
    expectEqual(outliers.front().toLongLong(),
                100,
                QStringLiteral("Outlier leaf should report the right outlier value."));

    widget.testSelectItem({0, 0, 0});
    const QString body = widget.testSummaryBody();
    expect(body.contains(QStringLiteral("Count 9"))
               && body.contains(QStringLiteral("Q1 12"))
               && body.contains(QStringLiteral("Median 14"))
               && body.contains(QStringLiteral("Q3 16"))
               && body.contains(QStringLiteral("Mean 23"))
               && body.contains(QStringLiteral("Outliers 1")),
           QStringLiteral("Summary panel should display the calculated outlier statistics."));
}

void testLatencyWidgetCongestedJitterDotsStayDark()
{
    CHIron::Gui::LatencyWidget widget;
    widget.setRows(buildLatencyWidgetCongestedJitterRows());
    showLatencyForTest(widget);
    widget.testSetPlotType(QStringLiteral("Jitter"));
    QApplication::processEvents();

    const QRect plot = widget.testPlotColumnWidgetRect();
    const auto [visibleMin, visibleMax] = widget.testPlotVisibleRange();
    const double span = qMax(1.0, visibleMax - visibleMin);
    const int trackLeft = plot.left() + 10;
    const int trackWidth = qMax(1, plot.width() - 20);
    const int clusterX = trackLeft + static_cast<int>(std::llround((20.0 - visibleMin) / span
                                                                   * static_cast<double>(trackWidth - 1)));
    const QRect clusterRegion(clusterX - 6, plot.top() + 8, 13, qMin(80, plot.height() - 16));
    const QImage image = renderLatencyImage(widget);

    int darkPixels = 0;
    int whitePixels = 0;
    const QRect bounded = clusterRegion.intersected(QRect(QPoint(0, 0), image.size()));
    for (int y = bounded.top(); y <= bounded.bottom(); ++y) {
        for (int x = bounded.left(); x <= bounded.right(); ++x) {
            const QColor pixel = QColor::fromRgba(image.pixel(x, y));
            if (pixel.red() < 90 && pixel.green() < 100 && pixel.blue() < 115 && pixel.alpha() > 220) {
                ++darkPixels;
            } else if (pixel.red() > 245 && pixel.green() > 245 && pixel.blue() > 245 && pixel.alpha() > 220) {
                ++whitePixels;
            }
        }
    }

    expectGreater(darkPixels,
                  20,
                  QStringLiteral("Dense jitter dots should retain visible dark dot pixels."));
    expect(darkPixels > whitePixels / 6,
           QStringLiteral("Dense jitter dots should not be washed out by white outlines."));
}

void testLatencyWidgetCursorSnapsToSparseJitterDot()
{
    CHIron::Gui::LatencyWidget widget;
    widget.setRows(buildLatencyWidgetTestRows());
    showLatencyForTest(widget);
    widget.testSetPlotType(QStringLiteral("Jitter"));
    QApplication::processEvents();

    int responseLeaf = -1;
    for (int index = 0; index < widget.testChildCount({0, 0}); ++index) {
        if (widget.testItemText({0, 0, index}, 0) == QStringLiteral("RSP CompDBIDResp")) {
            responseLeaf = index;
            break;
        }
    }
    expect(responseLeaf >= 0,
           QStringLiteral("Sparse cursor snap test should find the response leaf row."));

    const QPoint dot = widget.testJitterSamplePoint({0, 0, responseLeaf}, 0);
    expect(dot.x() > 0 && dot.y() > 0,
           QStringLiteral("Sparse cursor snap test should locate a rendered jitter dot."));
    widget.testSetPlotCursorFromPosition(dot + QPoint(6, 0));
    const QVariantMap state = widget.viewState();
    expect(state.value(QStringLiteral("plotHasCursor")).toBool(),
           QStringLiteral("Moving near a sparse jitter dot should show the latency cursor."));
    expectNear(state.value(QStringLiteral("plotCursorValue")).toDouble(),
               10.0,
               0.0001,
               QStringLiteral("Cursor should magnetically snap to a nearby sparse jitter dot."));
}

void testLatencyWidgetCursorDoesNotSnapToCongestedJitterDots()
{
    CHIron::Gui::LatencyWidget widget;
    widget.setRows(buildLatencyWidgetCongestedJitterRows());
    showLatencyForTest(widget);
    widget.testSetPlotType(QStringLiteral("Jitter"));
    QApplication::processEvents();

    const QPoint dot = widget.testJitterSamplePoint({0, 0, 0}, 0);
    expect(dot.x() > 0 && dot.y() > 0,
           QStringLiteral("Congested cursor snap test should locate a rendered jitter dot."));
    widget.testSetPlotCursorFromPosition(dot + QPoint(6, 0));
    const QVariantMap state = widget.viewState();
    expect(state.value(QStringLiteral("plotHasCursor")).toBool(),
           QStringLiteral("Moving inside a dense jitter cluster should still show the latency cursor."));
    expect(std::abs(state.value(QStringLiteral("plotCursorValue")).toDouble() - 20.0) > 0.001,
           QStringLiteral("Cursor should not snap inside a locally congested jitter dot cluster."));
}

void testLatencyWidgetJitterDotsActivateRelatedRows()
{
    CHIron::Gui::LatencyWidget widget;
    widget.setRows(buildLatencyWidgetTestRows());
    showLatencyForTest(widget);
    widget.testSetPlotType(QStringLiteral("Jitter"));
    QApplication::processEvents();

    std::vector<int> activatedRows;
    QObject::connect(&widget,
                     &CHIron::Gui::LatencyWidget::logicalRowActivated,
                     &widget,
                     [&](const int logicalRow) {
                         activatedRows.push_back(logicalRow);
                     });

    const QPoint parentDot = widget.testJitterSamplePoint({0}, 0);
    expect(parentDot.x() > 0 && parentDot.y() > 0,
           QStringLiteral("Parent latency row should expose its rendered completion dot."));
    expectEqual(widget.testPlotDotLogicalRowAtPosition(parentDot).value_or(-1),
                4,
                QStringLiteral("Parent completion dot should target the transaction request row."));
    widget.testBeginPlotLeftDrag(parentDot, Qt::NoModifier);
    widget.testFinishPlotLeftDrag(parentDot, Qt::NoModifier);
    expect(!activatedRows.empty() && activatedRows.back() == 4,
           QStringLiteral("Clicking a parent latency dot should jump to the transaction request row."));

    int responseLeaf = -1;
    int firstBeatLeaf = -1;
    for (int index = 0; index < widget.testChildCount({0, 0}); ++index) {
        const QString name = widget.testItemText({0, 0, index}, 0);
        if (name == QStringLiteral("RSP CompDBIDResp")) {
            responseLeaf = index;
        } else if (name == QStringLiteral("CompData first beat")) {
            firstBeatLeaf = index;
        }
    }
    expect(responseLeaf >= 0 && firstBeatLeaf >= 0,
           QStringLiteral("Dot activation test should find response and data leaf rows."));

    const QPoint responseDot = widget.testJitterSamplePoint({0, 0, responseLeaf}, 0);
    expectEqual(widget.testPlotDotLogicalRowAtPosition(responseDot).value_or(-1),
                1,
                QStringLiteral("Response latency dot should target its related response flit row."));
    widget.testBeginPlotLeftDrag(responseDot, Qt::NoModifier);
    widget.testFinishPlotLeftDrag(responseDot, Qt::NoModifier);
    expect(!activatedRows.empty() && activatedRows.back() == 1,
           QStringLiteral("Clicking a response latency dot should jump to the response flit."));

    const QPoint firstBeatDot = widget.testJitterSamplePoint({0, 0, firstBeatLeaf}, 0);
    expectEqual(widget.testPlotDotLogicalRowAtPosition(firstBeatDot).value_or(-1),
                2,
                QStringLiteral("Data first-beat latency dot should target its related data flit row."));
    widget.testBeginPlotLeftDrag(firstBeatDot, Qt::NoModifier);
    widget.testFinishPlotLeftDrag(firstBeatDot, Qt::NoModifier);
    expect(!activatedRows.empty() && activatedRows.back() == 2,
           QStringLiteral("Clicking a data latency dot should jump to the related data flit."));
}

void testLatencyWidgetRestoresSelectionExpansionScaleAndCursor()
{
    CHIron::Gui::LatencyWidget widget;
    widget.setRows(buildLatencyWidgetTestRows());
    showLatencyForTest(widget);

    QVariantMap state = widget.viewState();
    state.insert(QStringLiteral("plotType"), QStringLiteral("Violin"));
    state.insert(QStringLiteral("plotZoom"), 2.0);
    state.insert(QStringLiteral("plotCenter"), 20.0);
    state.insert(QStringLiteral("plotHasCursor"), true);
    state.insert(QStringLiteral("plotCursorValue"), 20.0);
    state.insert(QStringLiteral("selectedPath"),
                 QStringList{
                     QStringLiteral("Unknown Xaction"),
                     QStringLiteral("REQ ReadNoSnp"),
                     QStringLiteral("CompData first beat"),
                 });
    state.insert(QStringLiteral("expandedPaths"),
                 QStringList{
                     QStringLiteral("Unknown Xaction"),
                     QStringLiteral("Unknown Xaction/REQ ReadNoSnp"),
                 });

    widget.restoreViewState(state);
    QApplication::processEvents();

    const QVariantMap restored = widget.viewState();
    expectEqual(restored.value(QStringLiteral("plotType")).toString(),
                QStringLiteral("Violin"),
                QStringLiteral("Latency view state should restore plot type."));
    expectNear(restored.value(QStringLiteral("plotZoom")).toDouble(),
               2.0,
               0.0001,
               QStringLiteral("Latency view state should restore global scale zoom."));
    expectNear(restored.value(QStringLiteral("plotCenter")).toDouble(),
               20.0,
               0.0001,
               QStringLiteral("Latency view state should restore global scale center."));
    expect(restored.value(QStringLiteral("plotHasCursor")).toBool(),
           QStringLiteral("Latency view state should restore cursor visibility."));
    expectNear(restored.value(QStringLiteral("plotCursorValue")).toDouble(),
               20.0,
               0.0001,
               QStringLiteral("Latency view state should restore cursor value."));
    expectEqual(restored.value(QStringLiteral("selectedPath")).toStringList().join(QLatin1Char('/')),
                QStringLiteral("Unknown Xaction/REQ ReadNoSnp/CompData first beat"),
                QStringLiteral("Latency view state should restore selected bucket."));
    const QStringList expanded = restored.value(QStringLiteral("expandedPaths")).toStringList();
    expect(expanded.contains(QStringLiteral("Unknown Xaction/REQ ReadNoSnp")),
           QStringLiteral("Latency view state should restore expanded bucket nodes."));
}

void testLatencyWidgetDisplaysCursorTimeTagOnScale()
{
    CHIron::Gui::LatencyWidget widget;
    widget.setRows(buildLatencyWidgetTestRows());
    showLatencyForTest(widget);

    QVariantMap state = widget.viewState();
    state.insert(QStringLiteral("plotHasCursor"), true);
    state.insert(QStringLiteral("plotCursorValue"), 24.0);
    widget.restoreViewState(state);
    QApplication::processEvents();

    const QRect scale = widget.testScaleWidgetRect();
    expect(scale.width() > 160 && scale.height() >= 30,
           QStringLiteral("Latency scale should be large enough to render a cursor time tag."));
    const QRect tag = widget.testScaleCursorTagWidgetRect();
    expect(tag.isValid() && scale.contains(tag.center()),
           QStringLiteral("Latency scale should expose the cursor time tag inside the scale."));

    int darkPixels = 0;
    for (int x = tag.left(); x <= tag.right(); ++x) {
        for (int y = tag.top(); y <= tag.bottom(); ++y) {
            const QColor pixel = renderLatencyPixelAt(widget, QPoint(x, y));
            if (pixel.red() < 60 && pixel.green() < 70 && pixel.blue() < 90 && pixel.alpha() > 220) {
                ++darkPixels;
            }
        }
    }
    expectGreater(darkPixels,
                  32,
                  QStringLiteral("Latency scale should draw the cursor time tag at the cursor value."));
}

void testLatencyWidgetSwitchesPlotTypes()
{
    CHIron::Gui::LatencyWidget widget;
    widget.setRows(buildLatencyWidgetTestRows());
    showLatencyForTest(widget);

    const QRect plot = widget.testPlotColumnWidgetRect();
    expect(plot.width() > 160 && plot.height() > 100,
           QStringLiteral("Latency plot area should be large enough for plot type rendering tests."));
    const auto initialRange = widget.testPlotVisibleRange();

    for (const QString& plotType : {QStringLiteral("Box"), QStringLiteral("Jitter"), QStringLiteral("Violin")}) {
        widget.testSetPlotType(plotType);
        QApplication::processEvents();
        expectEqual(widget.testPlotType(),
                    plotType,
                    QStringLiteral("Latency plot type selector should switch to %1.").arg(plotType));
        expect(latencyRegionHasVisibleNonWhitePixel(widget, plot.adjusted(0, 0, 0, -34)),
               QStringLiteral("%1 plot mode should paint visible plot content.").arg(plotType));
        const auto range = widget.testPlotVisibleRange();
        expectNear(range.first,
                   initialRange.first,
                   0.0001,
                   QStringLiteral("%1 plot mode should keep the global scale minimum.").arg(plotType));
        expectNear(range.second,
                   initialRange.second,
                   0.0001,
                   QStringLiteral("%1 plot mode should keep the global scale maximum.").arg(plotType));
    }
}

void testLatencyWidgetLeftDragGesturesCrossRowsAndPaintOverlay()
{
    CHIron::Gui::LatencyWidget widget;
    widget.setRows(buildLatencyWidgetTestRows());
    showLatencyForTest(widget);

    const QRect plot = widget.testPlotColumnViewportRect();
    expect(plot.width() > 160 && plot.height() > 120,
           QStringLiteral("Latency plot column should be large enough for cross-row gesture tests."));

    const auto initialRange = widget.testPlotVisibleRange();
    const double initialSpan = initialRange.second - initialRange.first;
    const QPoint start(plot.left() + 60, plot.top() + 24);
    const QPoint horizontalEnd(plot.left() + 330, plot.top() + 86);
    widget.testBeginPlotLeftDrag(start, Qt::NoModifier);
    widget.testUpdatePlotLeftDrag(horizontalEnd, Qt::NoModifier);
    QApplication::processEvents();

    const QRect widgetPlot = widget.testPlotColumnWidgetRect();
    const QPoint overlayPoint(widgetPlot.left() + 190, widgetPlot.top() + 70);
    const QColor overlay = renderLatencyPixelAt(widget, overlayPoint);
    expect(overlay.blue() > overlay.red()
               && overlay.blue() > overlay.green()
               && overlay.red() > 180,
           QStringLiteral("Latency left-drag range gesture should paint a translucent blue overlay."));

    widget.testFinishPlotLeftDrag(horizontalEnd, Qt::NoModifier);
    QApplication::processEvents();
    const auto zoomedRange = widget.testPlotVisibleRange();
    expect((zoomedRange.second - zoomedRange.first) < initialSpan,
           QStringLiteral("Latency horizontal left drag should zoom to a selected range even when it crosses rows."));

    const double rangeZoom = widget.testPlotZoom();
    const QPoint diagonalStart(plot.left() + 180, plot.top() + 112);
    const QPoint diagonalEnd(diagonalStart.x() + 70, diagonalStart.y() - 70);
    widget.testBeginPlotLeftDrag(diagonalStart, Qt::NoModifier);
    widget.testUpdatePlotLeftDrag(diagonalEnd, Qt::NoModifier);
    widget.testFinishPlotLeftDrag(diagonalEnd, Qt::NoModifier);
    QApplication::processEvents();
    expectGreater(widget.testPlotZoom(),
                  rangeZoom,
                  QStringLiteral("Latency diagonal up-left drag should trigger the 2x zoom-in gesture."));

    const QPoint verticalStart(plot.left() + 260, plot.top() + 36);
    const QPoint verticalEnd(verticalStart.x(), verticalStart.y() + 96);
    widget.testBeginPlotLeftDrag(verticalStart, Qt::NoModifier);
    widget.testUpdatePlotLeftDrag(verticalEnd, Qt::NoModifier);
    widget.testFinishPlotLeftDrag(verticalEnd, Qt::NoModifier);
    QApplication::processEvents();
    expectNear(widget.testPlotZoom(),
               1.0,
               0.0001,
               QStringLiteral("Latency vertical left drag should trigger the plot-area Zoom full gesture."));
}

void testLatencyWidgetBuildsFromIndexedTraceSession()
{
    using XactTestConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 256, true, true, true>;
    using ReqFlit = CHI::Eb::Flits::REQ<XactTestConfig>;
    using DatFlit = CHI::Eb::Flits::DAT<XactTestConfig>;

    CLog::Parameters params;
    params.SetIssue(CLog::Issue::Eb);
    expect(params.SetNodeIdWidth(11), QStringLiteral("Failed to set test NodeIDWidth."));
    expect(params.SetReqAddrWidth(52), QStringLiteral("Failed to set test ReqAddrWidth."));
    expect(params.SetReqRSVDCWidth(32), QStringLiteral("Failed to set test ReqRSVDCWidth."));
    expect(params.SetDatRSVDCWidth(32), QStringLiteral("Failed to set test DatRSVDCWidth."));
    expect(params.SetDataWidth(256), QStringLiteral("Failed to set test DataWidth."));
    params.SetDataCheckPresent(true);
    params.SetPoisonPresent(true);
    params.SetMPAMPresent(true);

    ReqFlit readReq{};
    readReq.TgtID() = 2;
    readReq.SrcID() = 1;
    readReq.TxnID() = 7;
    readReq.Opcode() = CHI::Eb::Opcodes::REQ::ReadNoSnp;
    readReq.Size() = CHI::Eb::Sizes::B64;
    readReq.Addr() = 0x1000;
    readReq.AllowRetry() = 1;

    DatFlit compData0{};
    compData0.TgtID() = 1;
    compData0.SrcID() = 2;
    compData0.TxnID() = 7;
    compData0.HomeNID() = 2;
    compData0.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
    compData0.RespErr() = CHI::Eb::RespErrs::OK;
    compData0.Resp() = CHI::Eb::Resps::CompData_UC;
    compData0.DBID() = 9;
    compData0.DataID() = 0;
    compData0.BE() = 0xFFFFFFFFU;
    compData0.Data()[0] = 0x1112131415161718ULL;
    compData0.Data()[1] = 0x2122232425262728ULL;
    compData0.Data()[2] = 0x3132333435363738ULL;
    compData0.Data()[3] = 0x4142434445464748ULL;

    DatFlit compData2 = compData0;
    compData2.DataID() = 2;
    compData2.Data()[0] = 0x5152535455565758ULL;
    compData2.Data()[1] = 0x6162636465666768ULL;
    compData2.Data()[2] = 0x7172737475767778ULL;
    compData2.Data()[3] = 0x8182838485868788ULL;

    const auto serializeReq = [](const ReqFlit& flit, TestFlitBitWriter& writer) {
        appendEbReqFlitForTest(flit, writer);
    };
    const auto serializeDat = [](const DatFlit& flit, TestFlitBitWriter& writer) {
        appendEbDatFlitForTest(flit, writer);
    };

    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    const QString tracePath = tempDir.filePath(QStringLiteral("latency_widget_indexed_session.clogb"));
    writeTraceWithTopology(tracePath,
                           params,
                           {
                               {CLog::NodeType::RN_F, 1},
                               {CLog::NodeType::HN_F, 2},
                           },
                           {
                               makeSerializedRecord(CLog::Channel::TXREQ, 1, 1, readReq, serializeReq),
                               makeSerializedRecord(CLog::Channel::RXDAT, 1, 1, compData0, serializeDat),
                               makeSerializedRecord(CLog::Channel::RXDAT, 1, 1, compData2, serializeDat),
                           });

    CHIron::Gui::CLogBTraceLoadError error;
    std::shared_ptr<CHIron::Gui::TraceSession> session;
    expect(CHIron::Gui::TraceSession::open(tracePath, session, error),
           error.summary.isEmpty()
               ? QStringLiteral("Trace session should open the latency widget trace.")
               : error.summary);
    expect(session != nullptr, QStringLiteral("Trace session should be created."));
    expect(session->buildXactionIndex(error),
           error.summary.isEmpty()
               ? QStringLiteral("Persisted Xaction index should build for latency widget trace.")
               : error.summary);
    std::unordered_set<std::uint64_t> completedOrdinals;
    expect(session->xactionCompletedOrdinals(completedOrdinals),
           QStringLiteral("Latency widget trace should expose completed Xaction ordinals."));
    expectEqual(completedOrdinals.size(),
                static_cast<std::size_t>(1),
                QStringLiteral("The complete ReadNoSnp transaction should be marked complete once."));

    CHIron::Gui::LatencyWidget widget;
    widget.setTraceSession(session);
    widget.buildView();
    QElapsedTimer waitTimer;
    waitTimer.start();
    while (widget.testTopLevelCount() == 0 && waitTimer.elapsed() < 5000) {
        QApplication::processEvents();
        QThread::msleep(10);
    }

    expectEqual(widget.testTopLevelCount(),
                1,
                QStringLiteral("Latency widget should build buckets from a session-backed Xaction index."));
    expectEqual(widget.testChildCount({0}),
                1,
                QStringLiteral("Indexed session latency should classify by request opcode under the xaction type."));
    expectEqual(widget.testItemText({0, 0}, 0),
                QStringLiteral("REQ ReadNoSnp"),
                QStringLiteral("Indexed session latency should use the first REQ opcode as the request bucket."));
    expectEqual(widget.testChildCount({0, 0}),
                2,
                QStringLiteral("Indexed session latency should split first and following data beats."));
    expectLatencyStat(widget.testItemStats({0}),
                      QStringLiteral("count"),
                      1,
                      QStringLiteral("Indexed type row should count one completed transaction."));
    expectLatencyStat(widget.testItemStats({0}),
                      QStringLiteral("median"),
                      2,
                      QStringLiteral("Indexed type row should use transaction completion latency."));
    expectLatencyStat(widget.testItemStats({0, 0}),
                      QStringLiteral("count"),
                      1,
                      QStringLiteral("Indexed request row should count one completed transaction."));
    expectLatencyStat(widget.testItemStats({0, 0}),
                      QStringLiteral("median"),
                      2,
                      QStringLiteral("Indexed request row should use transaction completion latency."));

    QStringList leafNames;
    for (int index = 0; index < widget.testChildCount({0, 0}); ++index) {
        leafNames.append(widget.testItemText({0, 0, index}, 0));
        const QVariantMap leafStats = widget.testItemStats({0, 0, index});
        if (widget.testItemText({0, 0, index}, 0) == QStringLiteral("CompData first beat")) {
            expectLatencyStat(leafStats,
                              QStringLiteral("median"),
                              1,
                              QStringLiteral("First CompData leaf should keep the first beat latency."));
        } else if (widget.testItemText({0, 0, index}, 0) == QStringLiteral("CompData following beats")) {
            expectLatencyStat(leafStats,
                              QStringLiteral("median"),
                              2,
                              QStringLiteral("Following CompData leaf should keep the completion beat latency."));
        }
    }
    expect(leafNames.contains(QStringLiteral("CompData first beat")),
           QStringLiteral("Indexed session latency should show first CompData beat."));
    expect(leafNames.contains(QStringLiteral("CompData following beats")),
           QStringLiteral("Indexed session latency should show following CompData beats."));

    CHIron::Gui::LatencyWidget cachedWidget;
    cachedWidget.setTraceSession(session);
    cachedWidget.buildView();
    waitTimer.restart();
    while (cachedWidget.testTopLevelCount() == 0 && waitTimer.elapsed() < 5000) {
        QApplication::processEvents();
        QThread::msleep(10);
    }
    expectEqual(cachedWidget.testTopLevelCount(),
                1,
                QStringLiteral("Second indexed session latency build should still populate from the persisted cache."));
    expect(cachedWidget.testLastBuildLoadedFromCache(),
           QStringLiteral("Second indexed session latency build should report a persisted latency cache hit."));
}

void testLatencyWidgetIgnoresIncompleteIndexedTransactions()
{
    using XactTestConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 256, true, true, true>;
    using ReqFlit = CHI::Eb::Flits::REQ<XactTestConfig>;
    using DatFlit = CHI::Eb::Flits::DAT<XactTestConfig>;

    CLog::Parameters params;
    params.SetIssue(CLog::Issue::Eb);
    expect(params.SetNodeIdWidth(11), QStringLiteral("Failed to set test NodeIDWidth."));
    expect(params.SetReqAddrWidth(52), QStringLiteral("Failed to set test ReqAddrWidth."));
    expect(params.SetReqRSVDCWidth(32), QStringLiteral("Failed to set test ReqRSVDCWidth."));
    expect(params.SetDatRSVDCWidth(32), QStringLiteral("Failed to set test DatRSVDCWidth."));
    expect(params.SetDataWidth(256), QStringLiteral("Failed to set test DataWidth."));
    params.SetDataCheckPresent(true);
    params.SetPoisonPresent(true);
    params.SetMPAMPresent(true);

    ReqFlit completeReq{};
    completeReq.TgtID() = 2;
    completeReq.SrcID() = 1;
    completeReq.TxnID() = 7;
    completeReq.Opcode() = CHI::Eb::Opcodes::REQ::ReadNoSnp;
    completeReq.Size() = CHI::Eb::Sizes::B64;
    completeReq.Addr() = 0x1000;
    completeReq.AllowRetry() = 1;

    DatFlit compData0{};
    compData0.TgtID() = 1;
    compData0.SrcID() = 2;
    compData0.TxnID() = 7;
    compData0.HomeNID() = 2;
    compData0.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
    compData0.RespErr() = CHI::Eb::RespErrs::OK;
    compData0.Resp() = CHI::Eb::Resps::CompData_UC;
    compData0.DBID() = 9;
    compData0.DataID() = 0;
    compData0.BE() = 0xFFFFFFFFU;

    DatFlit compData2 = compData0;
    compData2.DataID() = 2;

    ReqFlit incompleteReq = completeReq;
    incompleteReq.TxnID() = 8;
    incompleteReq.Addr() = 0x2000;

    DatFlit incompleteFirstBeat = compData0;
    incompleteFirstBeat.TxnID() = 8;
    incompleteFirstBeat.DBID() = 10;
    incompleteFirstBeat.DataID() = 0;

    const auto serializeReq = [](const ReqFlit& flit, TestFlitBitWriter& writer) {
        appendEbReqFlitForTest(flit, writer);
    };
    const auto serializeDat = [](const DatFlit& flit, TestFlitBitWriter& writer) {
        appendEbDatFlitForTest(flit, writer);
    };

    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    const QString tracePath = tempDir.filePath(QStringLiteral("latency_completed_only.clogb"));
    writeTraceWithTopology(tracePath,
                           params,
                           {
                               {CLog::NodeType::RN_F, 1},
                               {CLog::NodeType::HN_F, 2},
                           },
                           {
                               makeSerializedRecord(CLog::Channel::TXREQ, 1, 1, completeReq, serializeReq),
                               makeSerializedRecord(CLog::Channel::RXDAT, 1, 1, compData0, serializeDat),
                               makeSerializedRecord(CLog::Channel::RXDAT, 1, 1, compData2, serializeDat),
                               makeSerializedRecord(CLog::Channel::TXREQ, 1, 10, incompleteReq, serializeReq),
                               makeSerializedRecord(CLog::Channel::RXDAT, 1, 10, incompleteFirstBeat, serializeDat),
                           });

    CHIron::Gui::CLogBTraceLoadError error;
    std::shared_ptr<CHIron::Gui::TraceSession> session;
    expect(CHIron::Gui::TraceSession::open(tracePath, session, error),
           error.summary.isEmpty()
               ? QStringLiteral("Trace session should open the completed-only latency trace.")
               : error.summary);
    expect(session != nullptr, QStringLiteral("Trace session should be created."));
    expect(session->buildXactionIndex(error),
           error.summary.isEmpty()
               ? QStringLiteral("Persisted Xaction index should build for completed-only latency trace.")
               : error.summary);

    std::unordered_set<std::uint64_t> completedOrdinals;
    expect(session->xactionCompletedOrdinals(completedOrdinals),
           QStringLiteral("Completed-only latency trace should expose completed ordinals."));
    expectEqual(completedOrdinals.size(),
                static_cast<std::size_t>(1),
                QStringLiteral("Only the transaction with both CompData beats should be complete."));

    CHIron::Gui::TraceSession::XactionIndexRowInfo completeTailInfo;
    expect(session->xactionRowInfo(2, completeTailInfo),
           QStringLiteral("Complete transaction tail row should have xaction info."));
    expect(completeTailInfo.xactionComplete,
           QStringLiteral("The second CompData beat should carry the completion marker."));

    CHIron::Gui::TraceSession::XactionIndexRowInfo incompleteTailInfo;
    expect(session->xactionRowInfo(4, incompleteTailInfo),
           QStringLiteral("Incomplete transaction row should still have xaction info."));
    expect(!incompleteTailInfo.xactionComplete,
           QStringLiteral("The truncated transaction should not carry the completion marker."));

    CHIron::Gui::LatencyWidget widget;
    widget.setTraceSession(session);
    widget.buildView();
    QElapsedTimer waitTimer;
    waitTimer.start();
    while (widget.testTopLevelCount() == 0 && waitTimer.elapsed() < 5000) {
        QApplication::processEvents();
        QThread::msleep(10);
    }

    expectEqual(widget.testTopLevelCount(),
                1,
                QStringLiteral("Latency widget should build from the completed transaction only."));
    expectEqual(widget.testChildCount({0}),
                1,
                QStringLiteral("Completed-only latency should have one request bucket."));
    expectEqual(widget.testChildCount({0, 0}),
                2,
                QStringLiteral("Incomplete transaction's first data beat must not add a latency sample."));
    expectLatencyStat(widget.testItemStats({0}),
                      QStringLiteral("count"),
                      1,
                      QStringLiteral("Completed-only type row should count only the completed transaction."));
    expectLatencyStat(widget.testItemStats({0}),
                      QStringLiteral("median"),
                      2,
                      QStringLiteral("Completed-only type row should use the completed transaction latency."));
    expectLatencyStat(widget.testItemStats({0, 0}),
                      QStringLiteral("count"),
                      1,
                      QStringLiteral("Completed-only request row should count only the completed transaction."));
    expectLatencyStat(widget.testItemStats({0, 0}),
                      QStringLiteral("median"),
                      2,
                      QStringLiteral("Completed-only request row should use the completed transaction latency."));

    QStringList leafNames;
    for (int index = 0; index < widget.testChildCount({0, 0}); ++index) {
        leafNames.append(widget.testItemText({0, 0, index}, 0));
    }
    expect(leafNames.contains(QStringLiteral("CompData first beat")),
           QStringLiteral("Completed transaction should contribute the first CompData beat."));
    expect(leafNames.contains(QStringLiteral("CompData following beats")),
           QStringLiteral("Completed transaction should contribute the following CompData beat."));
}

void testTransactionWidgetSkeletonStateAndActivation()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);

    expect(widget.focusPolicy() == Qt::StrongFocus,
           QStringLiteral("Transaction widget should keep local keyboard focus for waveform shortcuts."));
    expect(widget.toolTip().isEmpty()
               && widget.statusTip().contains(QStringLiteral("Click a span to select it"))
               && widget.statusTip().contains(QStringLiteral("Ctrl+wheel to zoom")),
           QStringLiteral("Transaction widget should expose concise interaction hints as status text, not native hover help."));
    expect(transactionImageHasVisibleContent(widget),
           QStringLiteral("Transaction widget empty state should render visible content."));
    QVariantMap emptyState = widget.viewState();
    expectEqual(emptyState.value(QStringLiteral("selectedLogicalRow")).toInt(),
                -1,
                QStringLiteral("Transaction widget should start without a selected logical row."));
    expect(!emptyState.value(QStringLiteral("hasCursor")).toBool(),
           QStringLiteral("Transaction widget should start without a visible cursor."));

    std::vector<CHIron::Gui::FlitRecord> rows = buildTimelineTestRows();
    widget.setRows(rows);
    QVariantMap rowState = widget.viewState();
    expect(rowState.value(QStringLiteral("sourceMode")).toString() == QStringLiteral("rowSnapshot"),
           QStringLiteral("Transaction widget row-snapshot source mode should be saved."));

    QVariantMap restoredState;
    restoredState.insert(QStringLiteral("sourceMode"), QStringLiteral("rowSnapshot"));
    restoredState.insert(QStringLiteral("selectedLogicalRow"), 7);
    restoredState.insert(QStringLiteral("hasCursor"), true);
    restoredState.insert(QStringLiteral("cursorTimestamp"), 12345);
    restoredState.insert(QStringLiteral("horizontalZoom"), 4.0);
    restoredState.insert(QStringLiteral("scrollOffset"), QVariant::fromValue<qulonglong>(42));
    widget.restoreViewState(restoredState);

    const QVariantMap restored = widget.viewState();
    expectEqual(restored.value(QStringLiteral("selectedLogicalRow")).toInt(),
                7,
                QStringLiteral("Transaction widget should restore selected logical row."));
    expect(restored.value(QStringLiteral("hasCursor")).toBool(),
           QStringLiteral("Transaction widget should restore cursor visibility."));
    expectEqual(static_cast<int>(restored.value(QStringLiteral("cursorTimestamp")).toLongLong()),
                12345,
                QStringLiteral("Transaction widget should restore cursor timestamp."));
    expectNear(restored.value(QStringLiteral("horizontalZoom")).toDouble(),
               4.0,
               0.0001,
               QStringLiteral("Transaction widget should restore horizontal zoom."));
    expectEqual(static_cast<int>(restored.value(QStringLiteral("scrollOffset")).toULongLong()),
                42,
                QStringLiteral("Transaction widget should restore scroll offset."));

    int activatedRow = -1;
    QObject::connect(&widget,
                     &CHIron::Gui::TransactionWidget::logicalRowActivated,
                     &widget,
                     [&activatedRow](const int logicalRow) {
                         activatedRow = logicalRow;
                     });
    QKeyEvent enterEvent(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QApplication::sendEvent(&widget, &enterEvent);
    QApplication::processEvents();
    expectEqual(activatedRow,
                7,
                QStringLiteral("Transaction widget should activate the selected logical row on Enter."));

    widget.clear();
    const QVariantMap cleared = widget.viewState();
    expectEqual(cleared.value(QStringLiteral("selectedLogicalRow")).toInt(),
                -1,
                QStringLiteral("Transaction widget clear should reset selection."));
    expect(!cleared.value(QStringLiteral("hasCursor")).toBool(),
           QStringLiteral("Transaction widget clear should hide the cursor."));
    expect(cleared.value(QStringLiteral("sourceMode")).toString() == QStringLiteral("empty"),
           QStringLiteral("Transaction widget clear should return to empty source mode."));
}

void testTransactionWidgetPolishHintsAndDiagnostics()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectSyntheticSpans(2, 6);
    QApplication::processEvents();

    expect(widget.toolTip().isEmpty(),
           QStringLiteral("Transaction widget should reserve hover for the transaction summary card."));
    expect(widget.statusTip().contains(QStringLiteral("Transaction spans ready: 6 spans across 2 stack rows."))
               && widget.statusTip().contains(QStringLiteral("drag vertically to fit")),
           QStringLiteral("Transaction widget status tip should combine state and gesture help."));

    widget.setSelectedLogicalRow(0);
    QApplication::processEvents();
    expect(widget.statusTip().contains(QStringLiteral("Selected: RN 0"))
               && widget.statusTip().contains(QStringLiteral("ReadNoSnp"))
               && !widget.statusTip().contains(QStringLiteral("REQ ReadNoSnp")),
           QStringLiteral("Selecting a Transaction span should enrich the widget status hint with selection details."));

    const QVariantMap diagnostics = widget.diagnosticsState();
    expectEqual(diagnostics.value(QStringLiteral("status")).toString(),
                QStringLiteral("Transaction spans ready: 6 spans across 2 stack rows."),
                QStringLiteral("Transaction diagnostics should expose the current status text."));
    expectEqual(diagnostics.value(QStringLiteral("spanCount")).toInt(),
                6,
                QStringLiteral("Transaction diagnostics should include span count."));
    expectEqual(diagnostics.value(QStringLiteral("laneCount")).toInt(),
                2,
                QStringLiteral("Transaction diagnostics should include stack-row count through the legacy laneCount field."));
    expectEqual(diagnostics.value(QStringLiteral("stackRowCount")).toInt(),
                2,
                QStringLiteral("Transaction diagnostics should include explicit stack-row count."));
    expectEqual(diagnostics.value(QStringLiteral("groupCount")).toInt(),
                2,
                QStringLiteral("Transaction diagnostics should include transaction node/origin group count."));
    expectEqual(diagnostics.value(QStringLiteral("maxStackDepth")).toInt(),
                1,
                QStringLiteral("Transaction diagnostics should include maximum inflight stack depth."));
    expectEqual(diagnostics.value(QStringLiteral("lastBuildWorkerCount")).toInt(),
                1,
                QStringLiteral("Transaction diagnostics should include the last span-build worker count."));
    expectEqual(diagnostics.value(QStringLiteral("selectedLogicalRow")).toInt(),
                0,
                QStringLiteral("Transaction diagnostics should include selected row state."));
    expectEqual(diagnostics.value(QStringLiteral("selectedOrdinal")).toULongLong(),
                static_cast<qulonglong>(0),
                QStringLiteral("Transaction diagnostics should include selected xaction ordinal."));
    expect(diagnostics.contains(QStringLiteral("visibleStart"))
               && diagnostics.contains(QStringLiteral("visibleEnd"))
               && diagnostics.contains(QStringLiteral("buildGeneration")),
           QStringLiteral("Transaction diagnostics should include visible range and build generation."));
    expect(diagnostics.contains(QStringLiteral("filterMode"))
               && diagnostics.contains(QStringLiteral("filterMatchCount"))
               && diagnostics.contains(QStringLiteral("unfilteredSpanCount")),
           QStringLiteral("Transaction diagnostics should include filter state."));
}

void testTransactionWidgetHeaderControlsDoNotOverlap()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectSyntheticSpans(2, 6);
    widget.setSelectedLogicalRow(0);
    QApplication::processEvents();

    const QRect switchRect = widget.testArrangeModeSwitchRect();
    const QRect toolbarRect = widget.testFilterToolbarRect();
    const QRect filterModeRect = widget.testFilterModeComboRect();
    const QRect opcodeFilterRect = widget.testOpcodeFilterEditRect();
    const QRect addressFilterRect = widget.testAddressFilterEditRect();
    const QRect txnIdFilterRect = widget.testTxnIdFilterEditRect();
    const QRect dbidFilterRect = widget.testDbidFilterEditRect();
    const QRect opcodeTagsRect = widget.testOpcodeTagsCheckBoxRect();
    const QRect hoverCardsRect = widget.testHoverCardsCheckBoxRect();
    const QRect selectionRect = widget.testHeaderSelectionTextRect();
    expect(switchRect.isValid(),
           QStringLiteral("Transaction arrangement switch should expose a valid header rectangle."));
    expect(filterModeRect.isValid()
               && opcodeFilterRect.isValid()
               && addressFilterRect.isValid()
               && txnIdFilterRect.isValid()
               && dbidFilterRect.isValid()
               && opcodeTagsRect.isValid()
               && hoverCardsRect.isValid(),
           QStringLiteral("Transaction filter toolbar controls should expose valid rectangles."));
    expect(toolbarRect.isValid()
               && toolbarRect.bottom() < widget.testPlotRect().top()
               && switchRect.bottom() < toolbarRect.top(),
           QStringLiteral("Transaction filter toolbar should sit below the header and above the plot."));
    expect(toolbarRect.contains(filterModeRect)
               && toolbarRect.contains(opcodeFilterRect)
               && toolbarRect.contains(addressFilterRect)
               && toolbarRect.contains(txnIdFilterRect)
               && toolbarRect.contains(dbidFilterRect)
               && toolbarRect.contains(opcodeTagsRect)
               && toolbarRect.contains(hoverCardsRect),
           QStringLiteral("Transaction filter controls should stay inside the top toolbar."));
    expect(selectionRect.isValid(),
           QStringLiteral("Transaction selected-row header text should have room at the normal widget width."));
    expect(!switchRect.intersects(selectionRect),
           QStringLiteral("Transaction selected-row header text should not overlap the Stack/TxnID switch."));
    expect(!toolbarRect.intersects(switchRect)
               && !opcodeFilterRect.intersects(addressFilterRect)
               && !addressFilterRect.intersects(txnIdFilterRect)
               && !txnIdFilterRect.intersects(dbidFilterRect)
               && !dbidFilterRect.intersects(opcodeTagsRect)
               && !opcodeTagsRect.intersects(hoverCardsRect),
           QStringLiteral("Transaction filter toolbar controls should not overlap each other or the arrangement switch."));

    widget.testSetBuildProgress(40, 100);
    QApplication::processEvents();
    const QRect progressRect = widget.testBuildProgressRect();
    const QRect buildingSelectionRect = widget.testHeaderSelectionTextRect();
    expect(progressRect.isValid(),
           QStringLiteral("Transaction build progress bar should remain visible at the normal widget width."));
    expect(!switchRect.intersects(progressRect),
           QStringLiteral("Transaction build progress bar should not overlap the Stack/TxnID switch."));
    expect(!buildingSelectionRect.isValid()
               || (!switchRect.intersects(buildingSelectionRect)
                   && !progressRect.intersects(buildingSelectionRect)),
           QStringLiteral("Transaction selected-row header text should stay between the switch and progress bar."));

    widget.resize(360, 260);
    QApplication::processEvents();
    const QRect narrowSwitchRect = widget.testArrangeModeSwitchRect();
    const QRect narrowProgressRect = widget.testBuildProgressRect();
    expect(!narrowProgressRect.isValid() || !narrowSwitchRect.intersects(narrowProgressRect),
           QStringLiteral("Transaction progress bar should shrink away instead of overlapping the switch on narrow headers."));
}

void testTransactionWidgetBuildProgressBarPaintsAndReportsProgress()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testSetBuildProgress(40, 100);
    QApplication::processEvents();

    const QRect progressRect = widget.testBuildProgressRect();
    expect(progressRect.isValid() && progressRect.width() >= 120 && progressRect.height() >= 10,
           QStringLiteral("Transaction progress bar should expose a visible build-progress rectangle."));

    const QVariantMap diagnostics = widget.diagnosticsState();
    expect(diagnostics.value(QStringLiteral("building")).toBool(),
           QStringLiteral("Transaction diagnostics should report active build progress."));
    expectEqual(diagnostics.value(QStringLiteral("buildProgressPhase")).toString(),
                QStringLiteral("scanning rows"),
                QStringLiteral("Transaction diagnostics should report the build progress phase."));
    expectEqual(diagnostics.value(QStringLiteral("buildProgressCompleted")).toInt(),
                40,
                QStringLiteral("Transaction diagnostics should report completed build progress work."));
    expectEqual(diagnostics.value(QStringLiteral("buildProgressTotal")).toInt(),
                100,
                QStringLiteral("Transaction diagnostics should report total build progress work."));

    const QImage image = renderTransactionImage(widget);
    int filledPixels = 0;
    int emptyPixels = 0;
    const int y = progressRect.center().y();
    for (int x = progressRect.left() + 2; x <= progressRect.right() - 2; ++x) {
        const QColor pixel = QColor::fromRgba(image.pixel(x, y));
        if (pixel.red() < 120 && pixel.green() > 140 && pixel.blue() > 170) {
            ++filledPixels;
        } else if (pixel.red() > 235 && pixel.green() > 235 && pixel.blue() > 235) {
            ++emptyPixels;
        }
    }
    expect(filledPixels > 20 && emptyPixels > 20,
           QStringLiteral("Transaction progress bar should paint both filled and remaining regions."));
}

void testTransactionWidgetBuildsSpansFromIndexedTraceSession()
{
    using XactTestConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 256, true, true, true>;
    using ReqFlit = CHI::Eb::Flits::REQ<XactTestConfig>;
    using DatFlit = CHI::Eb::Flits::DAT<XactTestConfig>;

    CLog::Parameters params;
    params.SetIssue(CLog::Issue::Eb);
    expect(params.SetNodeIdWidth(11), QStringLiteral("Failed to set test NodeIDWidth."));
    expect(params.SetReqAddrWidth(52), QStringLiteral("Failed to set test ReqAddrWidth."));
    expect(params.SetReqRSVDCWidth(32), QStringLiteral("Failed to set test ReqRSVDCWidth."));
    expect(params.SetDatRSVDCWidth(32), QStringLiteral("Failed to set test DatRSVDCWidth."));
    expect(params.SetDataWidth(256), QStringLiteral("Failed to set test DataWidth."));
    params.SetDataCheckPresent(true);
    params.SetPoisonPresent(true);
    params.SetMPAMPresent(true);

    ReqFlit readReq{};
    readReq.TgtID() = 2;
    readReq.SrcID() = 1;
    readReq.TxnID() = 7;
    readReq.Opcode() = CHI::Eb::Opcodes::REQ::ReadNoSnp;
    readReq.Size() = CHI::Eb::Sizes::B64;
    readReq.Addr() = 0x1000;
    readReq.AllowRetry() = 1;

    DatFlit compData0{};
    compData0.TgtID() = 1;
    compData0.SrcID() = 2;
    compData0.TxnID() = 7;
    compData0.HomeNID() = 2;
    compData0.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
    compData0.RespErr() = CHI::Eb::RespErrs::OK;
    compData0.Resp() = CHI::Eb::Resps::CompData_UC;
    compData0.DBID() = 9;
    compData0.DataID() = 0;
    compData0.BE() = 0xFFFFFFFFU;

    DatFlit compData2 = compData0;
    compData2.DataID() = 2;

    const auto serializeReq = [](const ReqFlit& flit, TestFlitBitWriter& writer) {
        appendEbReqFlitForTest(flit, writer);
    };
    const auto serializeDat = [](const DatFlit& flit, TestFlitBitWriter& writer) {
        appendEbDatFlitForTest(flit, writer);
    };

    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    const QString tracePath = tempDir.filePath(QStringLiteral("transaction_widget_spans.clogb"));
    writeTraceWithTopology(tracePath,
                           params,
                           {
                               {CLog::NodeType::RN_F, 1},
                               {CLog::NodeType::HN_F, 2},
                           },
                           {
                               makeSerializedRecord(CLog::Channel::TXREQ, 1, 2, readReq, serializeReq),
                               makeSerializedRecord(CLog::Channel::RXDAT, 1, 3, compData0, serializeDat),
                               makeSerializedRecord(CLog::Channel::RXDAT, 1, 5, compData2, serializeDat),
                           });

    CHIron::Gui::CLogBTraceLoadError error;
    std::shared_ptr<CHIron::Gui::TraceSession> session;
    expect(CHIron::Gui::TraceSession::open(tracePath, session, error),
           error.summary.isEmpty()
               ? QStringLiteral("Trace session should open the transaction widget span trace.")
               : error.summary);
    expect(session != nullptr, QStringLiteral("Trace session should be created."));
    expect(session->buildXactionIndex(error),
           error.summary.isEmpty()
               ? QStringLiteral("Persisted Xaction index should build for transaction widget span trace.")
               : error.summary);

    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.setTraceSession(session);
    waitForTransactionSpans(widget,
                            1,
                            QStringLiteral("Transaction widget should build one completed span from the indexed trace."));

    expectEqual(widget.testLaneCount(),
                1,
                QStringLiteral("The completed ReadNoSnp trace should create one transaction stack row."));
    const QVariantMap span = widget.testSpanAt(0);
    expect(span.value(QStringLiteral("completed")).toBool(),
           QStringLiteral("The span should inherit completed xaction metadata."));
    expectEqual(span.value(QStringLiteral("jointFamily")).toString(),
                QStringLiteral("RNFJoint"),
                QStringLiteral("ReadNoSnp trace should be classified as RNFJoint."));
    expectEqual(span.value(QStringLiteral("endpointLabel")).toString(),
                QStringLiteral("RN 1"),
                QStringLiteral("RNFJoint REQ span should classify by request SrcID."));
    expectEqual(span.value(QStringLiteral("classificationConfidence")).toString(),
                QStringLiteral("REQ SrcID"),
                QStringLiteral("RNFJoint REQ classification should report its endpoint source."));
    expectEqual(span.value(QStringLiteral("requestOpcode")).toString(),
                QStringLiteral("REQ ReadNoSnp"),
                QStringLiteral("The span should preserve the request opcode label."));
    expectEqual(span.value(QStringLiteral("originKind")).toString(),
                QStringLiteral("REQ"),
                QStringLiteral("RNFJoint REQ span should be classified under the REQ subgroup."));
    expectEqual(span.value(QStringLiteral("addressLabel")).toString(),
                QStringLiteral("0x000000001000"),
                QStringLiteral("The span should preserve the decoded request address label."));
    expectEqual(span.value(QStringLiteral("stackSlot")).toInt(),
                0,
                QStringLiteral("Single inflight transaction should use stack slot 0."));
    expectEqual(span.value(QStringLiteral("stackDepth")).toInt(),
                1,
                QStringLiteral("Single inflight transaction should report stack depth 1."));
    expectEqual(static_cast<int>(span.value(QStringLiteral("firstLogicalRow")).toULongLong()),
                0,
                QStringLiteral("The span should preserve the first logical row."));
    expectEqual(static_cast<int>(span.value(QStringLiteral("lastLogicalRow")).toULongLong()),
                2,
                QStringLiteral("The span should preserve the last logical row."));
    expectEqual(static_cast<int>(span.value(QStringLiteral("requestLogicalRow")).toULongLong()),
                0,
                QStringLiteral("The span should map activation to the request row."));
    expectEqual(static_cast<int>(span.value(QStringLiteral("completionLogicalRow")).toULongLong()),
                2,
                QStringLiteral("The span should preserve the completion row."));
    expectEqual(static_cast<int>(span.value(QStringLiteral("startTimestamp")).toLongLong()),
                2,
                QStringLiteral("The span start timestamp should come from the first xaction row."));
    expectEqual(static_cast<int>(span.value(QStringLiteral("endTimestamp")).toLongLong()),
                10,
                QStringLiteral("The span end timestamp should come from the completion row."));
    expectEqual(static_cast<int>(span.value(QStringLiteral("rowCount")).toULongLong()),
                3,
                QStringLiteral("The span should preserve related row count."));

    const QVariantMap lane = widget.testLaneAt(0);
    expectEqual(lane.value(QStringLiteral("endpointLabel")).toString(),
                QStringLiteral("RN 1"),
                QStringLiteral("The stack row should use the RN endpoint label."));
    expectEqual(lane.value(QStringLiteral("originKind")).toString(),
                QStringLiteral("REQ"),
                QStringLiteral("The stack row should preserve the REQ subgroup."));
    expectEqual(lane.value(QStringLiteral("stackDepth")).toInt(),
                1,
                QStringLiteral("The stack row should report group stack depth."));
}

void testTransactionWidgetArrangesRowsByTxnId()
{
    TransactionTraceFixture fixture =
        makeIndexedReadNoSnpTrace(QStringLiteral("transaction_widget_txnid_arrange.clogb"), 8);

    CHIron::Gui::CLogBTraceLoadError error;
    std::shared_ptr<CHIron::Gui::TraceSession> session;
    expect(CHIron::Gui::TraceSession::open(fixture.tracePath, session, error),
           error.summary.isEmpty()
               ? QStringLiteral("TxnID arrangement fixture should reopen.")
               : error.summary);
    expect(session != nullptr, QStringLiteral("TxnID arrangement session should be created."));
    expect(session->isXactionIndexComplete(),
           QStringLiteral("TxnID arrangement fixture should reuse the persisted xaction index."));

    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.setTraceSession(session);
    waitForTransactionSpans(widget,
                            fixture.transactionCount,
                            QStringLiteral("Transaction widget should build spans for TxnID arrangement."));

    expectEqual(widget.testArrangeModeName(),
                QStringLiteral("stack"),
                QStringLiteral("Transaction widget should default to inflight stack arrangement."));
    expectEqual(widget.testSpanAt(0).value(QStringLiteral("txnIdLabel")).toString(),
                QStringLiteral("0"),
                QStringLiteral("Transaction span should preserve TxnID from compact row metadata."));

    widget.testSetArrangeModeByName(QStringLiteral("txnId"));
    expectEqual(widget.testArrangeModeName(),
                QStringLiteral("txnId"),
                QStringLiteral("Transaction widget should switch to TxnID arrangement."));
    expectEqual(widget.testLaneCount(),
                fixture.transactionCount,
                QStringLiteral("TxnID arrangement should create one row for each TxnID in the fixture."));

    QSet<QString> observedTxnIds;
    for (int laneIndex = 0; laneIndex < widget.testLaneCount(); ++laneIndex) {
        const QVariantMap lane = widget.testLaneAt(laneIndex);
        observedTxnIds.insert(lane.value(QStringLiteral("txnIdLabel")).toString());
        expect(lane.value(QStringLiteral("label")).toString().startsWith(QStringLiteral("TxnID ")),
               QStringLiteral("TxnID arrangement lane labels should identify the TxnID."));
        expectEqual(static_cast<int>(lane.value(QStringLiteral("spanCount")).toULongLong()),
                    1,
                    QStringLiteral("Each unique TxnID row in the fixture should contain one transaction."));
    }
    for (int txnId = 0; txnId < fixture.transactionCount; ++txnId) {
        expect(observedTxnIds.contains(QString::number(txnId)),
               QStringLiteral("TxnID arrangement should expose every fixture TxnID."));
    }

    QVariantMap state = widget.viewState();
    expectEqual(state.value(QStringLiteral("arrangeMode")).toString(),
                QStringLiteral("txnId"),
                QStringLiteral("TxnID arrangement mode should be preserved in view state."));

    widget.testSetArrangeModeByName(QStringLiteral("stack"));
    expectEqual(widget.testArrangeModeName(),
                QStringLiteral("stack"),
                QStringLiteral("Transaction widget should switch back to inflight stack arrangement."));
    expect(widget.testLaneCount() < fixture.transactionCount,
           QStringLiteral("Inflight stack arrangement should pack non-overlapping transactions more tightly than TxnID rows."));
}

void testTransactionWidgetClassifiesSnfReqByTarget()
{
    using XactTestConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 256, true, true, true>;
    using ReqFlit = CHI::Eb::Flits::REQ<XactTestConfig>;
    using DatFlit = CHI::Eb::Flits::DAT<XactTestConfig>;

    CLog::Parameters params;
    params.SetIssue(CLog::Issue::Eb);
    expect(params.SetNodeIdWidth(11), QStringLiteral("Failed to set test NodeIDWidth."));
    expect(params.SetReqAddrWidth(52), QStringLiteral("Failed to set test ReqAddrWidth."));
    expect(params.SetReqRSVDCWidth(32), QStringLiteral("Failed to set test ReqRSVDCWidth."));
    expect(params.SetDatRSVDCWidth(32), QStringLiteral("Failed to set test DatRSVDCWidth."));
    expect(params.SetDataWidth(256), QStringLiteral("Failed to set test DataWidth."));
    params.SetDataCheckPresent(true);
    params.SetPoisonPresent(true);
    params.SetMPAMPresent(true);

    ReqFlit req{};
    req.TgtID() = 4;
    req.SrcID() = 1;
    req.TxnID() = 8;
    req.Opcode() = CHI::Eb::Opcodes::REQ::ReadNoSnp;
    req.Size() = CHI::Eb::Sizes::B64;
    req.Addr() = 0x2000;
    req.AllowRetry() = 1;

    DatFlit compData{};
    compData.TgtID() = 1;
    compData.SrcID() = 4;
    compData.TxnID() = 8;
    compData.HomeNID() = 4;
    compData.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
    compData.RespErr() = CHI::Eb::RespErrs::OK;
    compData.Resp() = CHI::Eb::Resps::CompData_UC;
    compData.DBID() = 13;
    compData.DataID() = 0;
    compData.BE() = 0xFFFFFFFFU;

    DatFlit compData2 = compData;
    compData2.DataID() = 2;

    const auto serializeReq = [](const ReqFlit& flit, TestFlitBitWriter& writer) {
        appendEbReqFlitForTest(flit, writer);
    };
    const auto serializeDat = [](const DatFlit& flit, TestFlitBitWriter& writer) {
        appendEbDatFlitForTest(flit, writer);
    };

    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    const QString tracePath = tempDir.filePath(QStringLiteral("transaction_widget_snf_req.clogb"));
    writeTraceWithTopology(tracePath,
                           params,
                           {
                               {CLog::NodeType::RN_F, 1},
                               {CLog::NodeType::SN_F, 3},
                               {CLog::NodeType::SN_F, 4},
                           },
                           {
                               makeSerializedRecord(CLog::Channel::RXREQ_BeforeSAM, 3, 1, req, serializeReq),
                               makeSerializedRecord(CLog::Channel::TXDAT, 3, 1, compData, serializeDat),
                               makeSerializedRecord(CLog::Channel::TXDAT, 3, 1, compData2, serializeDat),
                           });

    CHIron::Gui::CLogBTraceLoadError error;
    std::shared_ptr<CHIron::Gui::TraceSession> session;
    expect(CHIron::Gui::TraceSession::open(tracePath, session, error),
           error.summary.isEmpty()
               ? QStringLiteral("Trace session should open the SNF request transaction trace.")
               : error.summary);
    expect(session != nullptr, QStringLiteral("Trace session should be created."));
    expect(session->buildXactionIndex(error),
           error.summary.isEmpty()
               ? QStringLiteral("Persisted Xaction index should build for the SNF request transaction trace.")
               : error.summary);

    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.setTraceSession(session);
    waitForTransactionSpans(widget,
                            1,
                            QStringLiteral("Transaction widget should build one SNF request span."));

    const QVariantMap span = widget.testSpanAt(0);
    expectEqual(span.value(QStringLiteral("jointFamily")).toString(),
                QStringLiteral("SNFJoint"),
                QStringLiteral("BeforeSAM RXREQ from SN node should be classified as SNFJoint."));
    expectEqual(span.value(QStringLiteral("endpointLabel")).toString(),
                QStringLiteral("SN 4"),
                QStringLiteral("SNFJoint REQ span should classify by request TgtID."));
    expectEqual(span.value(QStringLiteral("classificationConfidence")).toString(),
                QStringLiteral("REQ TgtID"),
                QStringLiteral("SNFJoint REQ classification should report its endpoint source."));
    expectEqual(span.value(QStringLiteral("requestOpcode")).toString(),
                QStringLiteral("REQ ReadNoSnp"),
                QStringLiteral("SNFJoint span should preserve the request opcode label."));
    expectEqual(span.value(QStringLiteral("originKind")).toString(),
                QStringLiteral("REQ"),
                QStringLiteral("SNFJoint REQ span should be classified under the REQ subgroup."));
}

void testTransactionWidgetClassifiesRnfSnpByTarget()
{
    using XactTestConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 256, true, true, true>;
    using SnpFlit = CHI::Eb::Flits::SNP<XactTestConfig>;
    using RspFlit = CHI::Eb::Flits::RSP<XactTestConfig>;

    CLog::Parameters params;
    params.SetIssue(CLog::Issue::Eb);
    expect(params.SetNodeIdWidth(11), QStringLiteral("Failed to set test NodeIDWidth."));
    expect(params.SetReqAddrWidth(52), QStringLiteral("Failed to set test ReqAddrWidth."));
    expect(params.SetReqRSVDCWidth(32), QStringLiteral("Failed to set test ReqRSVDCWidth."));
    expect(params.SetDatRSVDCWidth(32), QStringLiteral("Failed to set test DatRSVDCWidth."));
    expect(params.SetDataWidth(256), QStringLiteral("Failed to set test DataWidth."));
    params.SetDataCheckPresent(true);
    params.SetPoisonPresent(true);
    params.SetMPAMPresent(true);

    SnpFlit snp{};
    snp.SrcID() = 10;
    snp.TxnID() = 21;
    snp.Opcode() = CHI::Eb::Opcodes::SNP::SnpMakeInvalid;
    snp.DoNotGoToSD() = 1;
    snp.Addr() = 0x3000 >> 3U;

    RspFlit response{};
    response.TgtID() = 10;
    response.SrcID() = 1;
    response.TxnID() = 21;
    response.Opcode() = CHI::Eb::Opcodes::RSP::SnpResp;
    response.RespErr() = CHI::Eb::RespErrs::OK;
    response.Resp() = CHI::Eb::Resps::Snoop::I;

    const auto serializeSnp = [](const SnpFlit& flit, TestFlitBitWriter& writer) {
        appendEbSnpFlitForTest(flit, writer);
    };
    const auto serializeRsp = [](const RspFlit& flit, TestFlitBitWriter& writer) {
        appendEbRspFlitForTest(flit, writer);
    };

    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    const QString tracePath = tempDir.filePath(QStringLiteral("transaction_widget_rnf_snp.clogb"));
    writeTraceWithTopology(tracePath,
                           params,
                           {
                               {CLog::NodeType::RN_F, 1},
                               {CLog::NodeType::HN_F, 10},
                           },
                           {
                               makeSerializedRecord(CLog::Channel::RXSNP, 1, 1, snp, serializeSnp),
                               makeSerializedRecord(CLog::Channel::TXRSP, 1, 2, response, serializeRsp),
                           });

    CHIron::Gui::CLogBTraceLoadError error;
    std::shared_ptr<CHIron::Gui::TraceSession> session;
    expect(CHIron::Gui::TraceSession::open(tracePath, session, error),
           error.summary.isEmpty()
               ? QStringLiteral("Trace session should open the RNF snoop transaction trace.")
               : error.summary);
    expect(session != nullptr, QStringLiteral("Trace session should be created."));
    expect(session->buildXactionIndex(error),
           error.summary.isEmpty()
               ? QStringLiteral("Persisted Xaction index should build for the RNF snoop transaction trace.")
               : error.summary);
    const std::vector<std::uint64_t> ordinals = session->xactionOrdinals();
    expectEqual(ordinals.size(),
                static_cast<std::size_t>(1),
                QStringLiteral("RNF snoop fixture should produce one indexed ordinal.\nRow 0:\n%1\nRow 1:\n%2")
                    .arg(session->xactionDebugInfo(0), session->xactionDebugInfo(1)));

    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.setTraceSession(session);
    waitForTransactionSpans(widget,
                            1,
                            QStringLiteral("Transaction widget should build one RNF snoop span."));

    const QVariantMap span = widget.testSpanAt(0);
    expectEqual(span.value(QStringLiteral("jointFamily")).toString(),
                QStringLiteral("RNFJoint"),
                QStringLiteral("RXSNP at an RN node should be classified as RNFJoint."));
    expectEqual(span.value(QStringLiteral("endpointLabel")).toString(),
                QStringLiteral("RN 1"),
                QStringLiteral("RNFJoint SNP span should classify by snoop target RN."));
    expectEqual(span.value(QStringLiteral("classificationConfidence")).toString(),
                QStringLiteral("SNP target context"),
                QStringLiteral("RNFJoint SNP classification should report the snoop target source."));
    expectEqual(span.value(QStringLiteral("requestOpcode")).toString(),
                QStringLiteral("SNP SnpMakeInvalid"),
                QStringLiteral("RNFJoint SNP span should use the snoop opcode label."));
    expectEqual(span.value(QStringLiteral("originKind")).toString(),
                QStringLiteral("SNP"),
                QStringLiteral("RNFJoint SNP span should be classified under the SNP subgroup."));
    expectEqual(static_cast<int>(span.value(QStringLiteral("requestLogicalRow")).toULongLong()),
                0,
                QStringLiteral("RNFJoint SNP span activation should target the snoop row."));
}

void testTransactionWidgetIgnoresDeniedUnindexedRows()
{
    using XactTestConfig = CHI::Eb::FlitConfiguration<11, 52, 32, 32, 256, true, true, true>;
    using ReqFlit = CHI::Eb::Flits::REQ<XactTestConfig>;

    CLog::Parameters params;
    params.SetIssue(CLog::Issue::Eb);
    expect(params.SetNodeIdWidth(11), QStringLiteral("Failed to set test NodeIDWidth."));
    expect(params.SetReqAddrWidth(52), QStringLiteral("Failed to set test ReqAddrWidth."));
    expect(params.SetReqRSVDCWidth(32), QStringLiteral("Failed to set test ReqRSVDCWidth."));
    expect(params.SetDatRSVDCWidth(32), QStringLiteral("Failed to set test DatRSVDCWidth."));
    expect(params.SetDataWidth(256), QStringLiteral("Failed to set test DataWidth."));
    params.SetDataCheckPresent(true);
    params.SetPoisonPresent(true);
    params.SetMPAMPresent(true);

    ReqFlit request{};
    request.TgtID() = 2;
    request.SrcID() = 1;
    request.TxnID() = 7;
    request.Opcode() = CHI::Eb::Opcodes::REQ::ReadNoSnp;
    request.Size() = CHI::Eb::Sizes::B64;
    request.Addr() = 0x1000;
    request.AllowRetry() = 1;

    using DatFlit = CHI::Eb::Flits::DAT<XactTestConfig>;
    DatFlit compData{};
    compData.TgtID() = 1;
    compData.SrcID() = 2;
    compData.TxnID() = 7;
    compData.HomeNID() = 2;
    compData.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
    compData.RespErr() = CHI::Eb::RespErrs::OK;
    compData.Resp() = CHI::Eb::Resps::CompData_UC;
    compData.DBID() = 9;
    compData.DataID() = 0;
    compData.BE() = 0xFFFFFFFFU;

    const auto serializeReq = [](const ReqFlit& flit, TestFlitBitWriter& writer) {
        appendEbReqFlitForTest(flit, writer);
    };
    const auto serializeDat = [](const DatFlit& flit, TestFlitBitWriter& writer) {
        appendEbDatFlitForTest(flit, writer);
    };

    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    const QString tracePath = tempDir.filePath(QStringLiteral("transaction_widget_denied_only.clogb"));
    writeTraceWithTopology(tracePath,
                           params,
                           {
                               {CLog::NodeType::RN_F, 1},
                               {CLog::NodeType::HN_F, 2},
                           },
                           {
                               makeSerializedRecord(CLog::Channel::TXREQ, 1, 1, request, serializeReq),
                               makeSerializedRecord(CLog::Channel::RXDAT, 2, 1, compData, serializeDat),
                           });

    CHIron::Gui::CLogBTraceLoadError error;
    std::shared_ptr<CHIron::Gui::TraceSession> session;
    expect(CHIron::Gui::TraceSession::open(tracePath, session, error),
           error.summary.isEmpty()
               ? QStringLiteral("Trace session should open the denied-only transaction trace.")
               : error.summary);
    expect(session != nullptr, QStringLiteral("Trace session should be created."));
    expect(session->buildXactionIndex(error),
           error.summary.isEmpty()
               ? QStringLiteral("Persisted Xaction index should build for denied-only transaction trace.")
               : error.summary);

    const std::vector<std::uint64_t> ordinals = session->xactionOrdinals();
    expectEqual(ordinals.size(),
                static_cast<std::size_t>(1),
                QStringLiteral("Only the accepted request should have an indexed transaction ordinal.\nRow 0:\n%1\nRow 1:\n%2")
                    .arg(session->xactionDebugInfo(0),
                         session->xactionDebugInfo(1)));

    CHIron::Gui::TraceSession::XactionIndexRowInfo deniedInfo;
    expect(session->xactionRowInfo(1, deniedInfo),
           QStringLiteral("Denied DAT row should still have persisted xaction metadata."));
    expect(deniedInfo.processed && !deniedInfo.indexed,
           QStringLiteral("Denied DAT row should be processed but not indexed."));

    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.setTraceSession(session);
    waitForTransactionSpans(widget,
                            1,
                            QStringLiteral("Transaction widget should build spans only from indexed ordinals."));

    const QVariantMap span = widget.testSpanAt(0);
    expectEqual(static_cast<int>(span.value(QStringLiteral("firstLogicalRow")).toULongLong()),
                0,
                QStringLiteral("The accepted request should be the only transaction span."));
    expectEqual(static_cast<int>(span.value(QStringLiteral("lastLogicalRow")).toULongLong()),
                0,
                QStringLiteral("Denied/unindexed rows must not be folded into the span."));
}

void testTransactionWidgetLargeBuildUsesCompactRows()
{
    const int transactionCount = 768;
    TransactionTraceFixture fixture =
        makeIndexedReadNoSnpTrace(QStringLiteral("transaction_large_compact.clogb"), transactionCount);

    CHIron::Gui::TraceSessionOptions options;
    options.maxCachedBlocks = 1;
    options.maxCachedPages = 1;
    options.pageSize = 8;

    CHIron::Gui::CLogBTraceLoadError error;
    std::shared_ptr<CHIron::Gui::TraceSession> session;
    expect(CHIron::Gui::TraceSession::open(fixture.tracePath, session, error, options),
           error.summary.isEmpty()
               ? QStringLiteral("Large compact transaction fixture should reopen.")
               : error.summary);
    expect(session != nullptr, QStringLiteral("Large compact transaction session should be created."));
    expect(session->isXactionIndexComplete(),
           QStringLiteral("Large compact transaction fixture should reuse the persisted xaction index."));
    expectEqual(static_cast<int>(session->cachedPageCount()),
                0,
                QStringLiteral("Large compact transaction session should start with an empty decoded-row page cache."));

    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);

    QElapsedTimer buildTimer;
    buildTimer.start();
    widget.setTraceSession(session);
    waitForTransactionSpans(widget,
                            fixture.transactionCount,
                            QStringLiteral("Large transaction fixture should build one span per completed transaction."));
    const qint64 elapsedMs = buildTimer.elapsed();

    expectEqual(widget.testLaneCount(),
                4,
                QStringLiteral("Large compact transaction build should classify non-overlapping REQ spans into RN stack rows."));
    expectEqual(static_cast<int>(session->cachedPageCount()),
                0,
                QStringLiteral("Transaction span building should not populate decoded-row page cache when compact metadata is available."));
    expect(session->cachedBlockCount() <= session->maxCachedBlocks(),
           QStringLiteral("Transaction span building should keep the block cache bounded."));
    expect(elapsedMs < 5000,
           QStringLiteral("Large compact transaction span build should complete promptly in the regression fixture."));
    expect(widget.testLastBuildWorkerCount() >= 1U && widget.testLastBuildWorkerCount() <= 8U,
           QStringLiteral("Large compact transaction span build should report a bounded worker count."));
    expectEqual(widget.diagnosticsState().value(QStringLiteral("lastBuildWorkerCount")).toInt(),
                static_cast<int>(widget.testLastBuildWorkerCount()),
                QStringLiteral("Transaction diagnostics should report the worker count used by the last build."));

    const QVariantMap firstSpan = widget.testSpanAt(0);
    expectEqual(static_cast<int>(firstSpan.value(QStringLiteral("rowCount")).toULongLong()),
                3,
                QStringLiteral("Compact transaction span should preserve xaction row count."));
    expectEqual(static_cast<int>(firstSpan.value(QStringLiteral("firstLogicalRow")).toULongLong()),
                0,
                QStringLiteral("Compact transaction span should preserve the first row."));
    expectEqual(static_cast<int>(firstSpan.value(QStringLiteral("lastLogicalRow")).toULongLong()),
                2,
                QStringLiteral("Compact transaction span should preserve the last row."));
    expectEqual(static_cast<int>(firstSpan.value(QStringLiteral("startTimestamp")).toLongLong()),
                1,
                QStringLiteral("Compact transaction span start timestamp should come from fast metadata."));
    expectEqual(static_cast<int>(firstSpan.value(QStringLiteral("endTimestamp")).toLongLong()),
                3,
                QStringLiteral("Compact transaction span completion timestamp should come from fast metadata."));

    const QString relatedRows = widget.testRelatedRowsTextForSpan(0);
    expect(relatedRows.contains(QStringLiteral("Rows: 0, 1, 2")),
           QStringLiteral("Related-row details should lazily recover all rows for a compact span."));
}

void testTransactionWidgetBuildCancellationRemainsResponsive()
{
    TransactionTraceFixture fixture =
        makeIndexedReadNoSnpTrace(QStringLiteral("transaction_cancel_build.clogb"), 2048);

    CHIron::Gui::CLogBTraceLoadError error;
    std::shared_ptr<CHIron::Gui::TraceSession> session;
    expect(CHIron::Gui::TraceSession::open(fixture.tracePath, session, error),
           error.summary.isEmpty()
               ? QStringLiteral("Cancellation transaction fixture should reopen.")
               : error.summary);
    expect(session != nullptr, QStringLiteral("Cancellation transaction session should be created."));

    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);

    QElapsedTimer cancelTimer;
    cancelTimer.start();
    widget.setTraceSession(session);
    widget.buildView();
    QApplication::processEvents();
    widget.clear();
    QApplication::processEvents();
    expect(cancelTimer.elapsed() < 1000,
           QStringLiteral("Clearing a building Transaction widget should return promptly."));
    expectEqual(widget.testSpanCount(),
                0,
                QStringLiteral("Cleared Transaction widget should not retain partially built spans."));
    expect(waitForCondition([&widget]() {
               QApplication::processEvents();
               return widget.testSpanCount() == 0;
           }, 300),
           QStringLiteral("Late Transaction build completion should not repopulate a cleared widget."));
}

void testTransactionWidgetTimelineRenderingPaintsBarsAndRuler()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectSyntheticSpans(3, 9);
    QApplication::processEvents();

    expectEqual(widget.testSpanCount(),
                9,
                QStringLiteral("Synthetic transaction render fixture should create spans."));
    expectEqual(widget.testLaneCount(),
                3,
                QStringLiteral("Synthetic transaction render fixture should create stack rows."));

    const QVariantMap layout = widget.testLayoutState();
    expect(layout.value(QStringLiteral("plotWidth")).toInt() > 500,
           QStringLiteral("Transaction plot should reserve a wide drawing region."));
    expect(layout.value(QStringLiteral("plotHeight")).toInt() > 120,
           QStringLiteral("Transaction plot should reserve a readable row region."));
    expect(layout.value(QStringLiteral("rulerTop")).toInt()
               >= layout.value(QStringLiteral("plotTop")).toInt() + layout.value(QStringLiteral("plotHeight")).toInt(),
           QStringLiteral("Transaction ruler should be separated below the plot body."));
    expect(layout.value(QStringLiteral("laneHeaderWidth")).toInt() > 0,
           QStringLiteral("Transaction stack-row headers should occupy a dedicated region."));

    const QPoint firstSpanCenter = widget.testSpanCenterPoint(0);
    expect(firstSpanCenter != QPoint(),
           QStringLiteral("First transaction span should have a visible center point."));
    expectEqual(widget.testSpanIndexAtPoint(firstSpanCenter),
                0,
                QStringLiteral("Hit testing should find the painted first transaction span."));
    expect(transactionImageHasNonWhiteNear(widget, firstSpanCenter, 4),
           QStringLiteral("Transaction render should paint a non-white bar at the span center."));

    const QPoint secondLaneSpanCenter = widget.testSpanCenterPoint(1);
    expect(secondLaneSpanCenter != QPoint(),
           QStringLiteral("Second lane transaction span should have a visible center point."));
    expectEqual(widget.testSpanIndexAtPoint(secondLaneSpanCenter),
                1,
           QStringLiteral("Hit testing should respect transaction stack-row Y coordinates."));
}

void testTransactionWidgetRulerAndInfoBarDoNotOverlap()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectSyntheticSpans(3, 9);
    QApplication::processEvents();

    const QPoint firstSpanCenter = widget.testSpanCenterPoint(0);
    widget.testSetCursorFromPosition(firstSpanCenter);
    widget.setSelectedLogicalRow(0);
    QApplication::processEvents();

    const QRect plot = widget.testPlotRect();
    const QRect ruler = widget.testRulerRect();
    const QRect infoBar = widget.testInfoBarRect();
    const QVariantMap layout = widget.testLayoutState();

    expect(plot.isValid() && ruler.isValid() && infoBar.isValid(),
           QStringLiteral("Transaction plot, ruler, and info bar should all have valid geometry."));
    expect(plot.bottom() < ruler.top(),
           QStringLiteral("Transaction scale ruler should be below the plot body."));
    expect(ruler.bottom() < infoBar.top(),
           QStringLiteral("Transaction info bar should be separated below the scale ruler."));
    expect(!ruler.intersects(infoBar),
           QStringLiteral("Transaction scale ruler should not overlap the bottom info bar."));
    expectEqual(layout.value(QStringLiteral("infoBarTop")).toInt(),
                infoBar.top(),
                QStringLiteral("Transaction layout diagnostics should expose the info-bar top."));
    expect(layout.value(QStringLiteral("infoBarTop")).toInt()
               >= layout.value(QStringLiteral("rulerTop")).toInt()
                      + layout.value(QStringLiteral("rulerHeight")).toInt(),
           QStringLiteral("Transaction layout diagnostics should keep the info bar after the ruler."));
}

void testTransactionWidgetDisplaysCursorTimeTagOnRuler()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectSyntheticSpans(3, 9);
    QApplication::processEvents();

    const QPoint firstSpanCenter = widget.testSpanCenterPoint(0);
    widget.testSetCursorFromPosition(firstSpanCenter);
    QApplication::processEvents();

    const QRect ruler = widget.testRulerRect();
    const QRect tag = widget.testCursorRulerTagRect();
    expect(widget.testHasCursor(),
           QStringLiteral("Transaction cursor should be visible before checking the ruler tag."));
    expect(tag.isValid() && ruler.contains(tag.center()),
           QStringLiteral("Transaction ruler should expose a cursor time tag inside the ruler."));

    const QImage image = renderTransactionImage(widget);
    int darkPixels = 0;
    for (int x = tag.left(); x <= tag.right(); ++x) {
        for (int y = tag.top(); y <= tag.bottom(); ++y) {
            const QColor pixel = QColor::fromRgba(image.pixel(qBound(0, x, image.width() - 1),
                                                              qBound(0, y, image.height() - 1)));
            if (pixel.red() < 40 && pixel.green() < 45 && pixel.blue() < 55 && pixel.alpha() > 220) {
                ++darkPixels;
            }
        }
    }
    expectGreater(darkPixels,
                  48,
                  QStringLiteral("Transaction ruler should draw a dark cursor time tag at the cursor."));
}

void testTransactionWidgetLifetimeStackOverlapLayoutAndLabels()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectOverlappingSyntheticSpans();
    QApplication::processEvents();

    expectEqual(widget.testSpanCount(),
                5,
                QStringLiteral("Overlap fixture should create lifetime rectangles including unknown-origin cases."));
    expectEqual(widget.testLaneCount(),
                4,
                QStringLiteral("Overlap fixture should create stack rows, not opcode summary lanes."));

    const QVariantMap layout = widget.testLayoutState();
    expectEqual(layout.value(QStringLiteral("groupCount")).toInt(),
                3,
                QStringLiteral("Overlap layout should group by node and REQ/SNP origin."));
    expectEqual(layout.value(QStringLiteral("stackRowCount")).toInt(),
                4,
                QStringLiteral("Overlap layout should expose total stack-row count."));
    expectEqual(layout.value(QStringLiteral("maxStackDepth")).toInt(),
                2,
                QStringLiteral("Overlapping transactions should increase inflight stack depth."));

    const QVariantMap first = widget.testSpanAt(0);
    const QVariantMap overlapping = widget.testSpanAt(1);
    const QVariantMap reused = widget.testSpanAt(2);
    const QVariantMap snp = widget.testSpanAt(3);
    const QVariantMap unknown = widget.testSpanAt(4);
    expectEqual(first.value(QStringLiteral("originKind")).toString(),
                QStringLiteral("REQ"),
                QStringLiteral("First synthetic span should be in the REQ subgroup."));
    expectEqual(first.value(QStringLiteral("stackSlot")).toInt(),
                0,
                QStringLiteral("First span should take the first free stack slot."));
    expectEqual(overlapping.value(QStringLiteral("stackSlot")).toInt(),
                1,
                QStringLiteral("Overlapping span should be piled into a different stack slot."));
    expectEqual(reused.value(QStringLiteral("stackSlot")).toInt(),
                0,
                QStringLiteral("Non-overlapping later span should reuse the first stack slot deterministically."));
    expectEqual(snp.value(QStringLiteral("originKind")).toString(),
                QStringLiteral("SNP"),
                QStringLiteral("SNP-origin span should be classified under the SNP subgroup."));
    expectEqual(snp.value(QStringLiteral("endpointLabel")).toString(),
                QStringLiteral("RN 1"),
                QStringLiteral("SNP synthetic span should remain under its RN node group."));
    expectEqual(first.value(QStringLiteral("addressLabel")).toString(),
                QStringLiteral("0x000000001000"),
                QStringLiteral("Lifetime rectangle details should keep the decoded address label."));
    expectEqual(unknown.value(QStringLiteral("jointFamily")).toString(),
                QStringLiteral("Unknown Joint"),
                QStringLiteral("Unknown-origin transactions should remain visible."));
    expectEqual(unknown.value(QStringLiteral("originKind")).toString(),
                QStringLiteral("Unknown"),
                QStringLiteral("Unknown-origin transactions should be grouped under Unknown."));

    const QVariantMap firstLane = widget.testLaneAt(0);
    const QVariantMap secondLane = widget.testLaneAt(1);
    const QVariantMap snpLane = widget.testLaneAt(2);
    const QVariantMap unknownLane = widget.testLaneAt(3);
    expectEqual(firstLane.value(QStringLiteral("groupLabel")).toString(),
                QStringLiteral("RN 0 / REQ"),
                QStringLiteral("First stack row header should be node first, then REQ."));
    expectEqual(firstLane.value(QStringLiteral("stackDepth")).toInt(),
                2,
                QStringLiteral("First group should expose its stack depth."));
    expectEqual(secondLane.value(QStringLiteral("stackSlot")).toInt(),
                1,
                QStringLiteral("Second stack row should represent stack slot 1."));
    expectEqual(snpLane.value(QStringLiteral("groupLabel")).toString(),
                QStringLiteral("RN 1 / SNP"),
                QStringLiteral("SNP stack row header should be node first, then SNP."));
    expectEqual(unknownLane.value(QStringLiteral("groupLabel")).toString(),
                QStringLiteral("Unknown endpoint / Unknown"),
                QStringLiteral("Unknown stack row should be visible under an unknown group."));

    const QPoint firstCenter = widget.testSpanCenterPoint(0);
    const QPoint overlappingCenter = widget.testSpanCenterPoint(1);
    expect(firstCenter != QPoint() && overlappingCenter != QPoint(),
           QStringLiteral("Overlapping lifetime rectangles should both be visible."));
    expect(std::abs(firstCenter.y() - overlappingCenter.y()) >= layout.value(QStringLiteral("rowHeight")).toInt() - 2,
           QStringLiteral("Overlapping transactions should be piled vertically on distinct stack rows."));
    expect(transactionImageHasNonWhiteNear(widget, firstCenter, 5)
               && transactionImageHasNonWhiteNear(widget, overlappingCenter, 5),
           QStringLiteral("Lifetime stack renderer should paint the overlapping rectangles."));
}

void testTransactionWidgetEdaStyleUsesFullRowRectanglesAndInlineLabels()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectOverlappingSyntheticSpans();
    QApplication::processEvents();

    const QVariantMap layout = widget.testLayoutState();
    expectEqual(layout.value(QStringLiteral("rowHeight")).toInt(),
                32,
                QStringLiteral("EDA-style transaction rows should remain tall enough for readable rectangles."));

    const QRect firstRect = widget.testSpanVisualRect(0);
    expect(firstRect.isValid(),
           QStringLiteral("EDA-style fixture should expose a visible transaction rectangle."));
    expectEqual(firstRect.height(),
                layout.value(QStringLiteral("rowHeight")).toInt(),
                QStringLiteral("Transaction lifetime rectangles should occupy the full stack-row height."));
    expectEqual(widget.testSpanLabelFontFamily(),
                QStringLiteral("Consolas"),
                QStringLiteral("Transaction rectangle labels should use the Consolas font family."));
    expectEqual(widget.testSpanLabelFontPixelSize(),
                11,
                QStringLiteral("Default transaction rectangle labels should use the maximum compact font size."));

    const QString wideLabel = widget.testInlineLabelForSpan(0, 210);
    expect(wideLabel.contains(QStringLiteral("ReadNoSnp"))
               && wideLabel.contains(QStringLiteral("0x000000001000"))
               && !wideLabel.contains(QStringLiteral("REQ ")),
           QStringLiteral("Transaction rectangles should expose opcode and address labels without the channel prefix when space allows."));
    const QString narrowLabel = widget.testInlineLabelForSpan(0, 70);
    expectEqual(narrowLabel,
                QStringLiteral("ReadNoSnp"),
                QStringLiteral("Narrow transaction rectangles should render opcode only when the address does not fit."));
    const QString elidedLabel = widget.testInlineLabelForSpan(0, 20);
    expect(!elidedLabel.isEmpty()
               && elidedLabel.startsWith(QStringLiteral("R"))
               && elidedLabel.size() < QStringLiteral("ReadNoSnp").size(),
           QStringLiteral("Transaction rectangles should elide opcode labels when the full opcode does not fit."));
    expect(widget.testInlineLabelForSpan(0, 1).isEmpty(),
           QStringLiteral("Transaction rectangles should not render labels when even one opcode character does not fit."));

    expectEqual(widget.testSpanFillColorName(0),
                QStringLiteral("#FCEBD5"),
                QStringLiteral("REQ transaction fill should be a light tint of the trace-table badge color."));
    expectEqual(widget.testSpanEdgeColorName(0),
                CHIron::Gui::ChannelAccent(CHIron::Gui::FlitChannel::Req).name(QColor::HexRgb).toUpper(),
                QStringLiteral("REQ transaction edge should match the trace-table channel badge accent exactly."));
    const QPoint center = firstRect.center();
    expect(transactionImageHasColorNear(widget, center, 14, [](const QColor& pixel) {
               return pixel.red() >= 238 && pixel.green() >= 218 && pixel.blue() >= 190
                   && pixel.red() > pixel.blue()
                   && pixel != QColor(Qt::white);
           }),
           QStringLiteral("REQ transaction lifetime rectangle should render a pale tint of the trace-table badge color."));
    const QPoint topEdge(firstRect.center().x(), firstRect.top() + 2);
    expect(transactionImageHasColorNear(widget, topEdge, 5, [](const QColor& pixel) {
               return pixel.red() >= 230 && pixel.green() >= 145 && pixel.green() <= 185 && pixel.blue() <= 90;
           }),
           QStringLiteral("REQ transaction rectangle should paint the trace-table channel badge color on the edge."));

    const QString snpLabel = widget.testInlineLabelForSpan(3, 210);
    expect(snpLabel.contains(QStringLiteral("SnpOnce"))
               && snpLabel.contains(QStringLiteral("0x000000004000"))
               && !snpLabel.contains(QStringLiteral("SNP ")),
           QStringLiteral("SNP transaction labels should also omit the channel prefix."));
    const QRect snpRect = widget.testSpanVisualRect(3);
    expect(snpRect.isValid(),
           QStringLiteral("SNP rectangle should be visible for channel-color verification."));
    expectEqual(widget.testSpanFillColorName(3),
                QStringLiteral("#F9E0DE"),
                QStringLiteral("SNP transaction fill should be a light tint of the trace-table badge color."));
    expectEqual(widget.testSpanEdgeColorName(3),
                CHIron::Gui::ChannelAccent(CHIron::Gui::FlitChannel::Snp).name(QColor::HexRgb).toUpper(),
                QStringLiteral("SNP transaction edge should match the trace-table channel badge accent exactly."));
    expect(transactionImageHasColorNear(widget, QPoint(snpRect.center().x(), snpRect.top() + 2), 5, [](const QColor& pixel) {
               return pixel.red() >= 220 && pixel.green() >= 95 && pixel.green() <= 135 && pixel.blue() >= 90 && pixel.blue() <= 130;
           }),
           QStringLiteral("SNP transaction rectangle should paint the trace-table channel badge color on the edge."));
}

void testTransactionWidgetLaneHeaderDisplaysNodeAndChannelBadges()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectOverlappingSyntheticSpans();
    QApplication::processEvents();

    const QVariantMap reqLane = widget.testLaneAt(0);
    const QVariantMap snpLane = widget.testLaneAt(2);
    expectEqual(reqLane.value(QStringLiteral("endpointNodeType")).toString(),
                QStringLiteral("RN-F"),
                QStringLiteral("REQ lane should expose a node type for the left-column badge."));
    expectEqual(snpLane.value(QStringLiteral("endpointNodeType")).toString(),
                QStringLiteral("RN-F"),
                QStringLiteral("SNP lane should expose a node type for the left-column badge."));
    expectEqual(widget.testLaneNodeBadgeColorName(0),
                QStringLiteral("#2F80ED"),
                QStringLiteral("Transaction node badge should use the existing RN-F badge color."));
    expectEqual(widget.testLaneChannelBadgeColorName(0),
                CHIron::Gui::ChannelAccent(CHIron::Gui::FlitChannel::Req).name(QColor::HexRgb).toUpper(),
                QStringLiteral("Transaction REQ badge should use the trace-table REQ color."));
    expectEqual(widget.testLaneChannelBadgeColorName(2),
                CHIron::Gui::ChannelAccent(CHIron::Gui::FlitChannel::Snp).name(QColor::HexRgb).toUpper(),
                QStringLiteral("Transaction SNP badge should use the trace-table SNP color."));

    const QRect nodeBadge = widget.testLaneNodeBadgeRect(0);
    const QRect nodeIdRect = widget.testLaneNodeIdRect(0);
    const QRect reqBadge = widget.testLaneChannelBadgeRect(0);
    const QRect snpBadge = widget.testLaneChannelBadgeRect(2);
    const int titleFontSize = widget.testLaneHeaderTitleFontPixelSize();
    const int rowFontSize = widget.testLaneHeaderRowFontPixelSize();
    const int titleCountFontSize = widget.testLaneHeaderCountFontPixelSize(0);
    const int continuationCountFontSize = widget.testLaneHeaderCountFontPixelSize(1);
    expect(nodeBadge.isValid() && nodeIdRect.isValid() && reqBadge.isValid() && snpBadge.isValid(),
           QStringLiteral("Transaction lane header should expose node badge, node ID, and channel badge rectangles."));
    expectEqual(nodeBadge.height(),
                16,
                QStringLiteral("Transaction lane-header badges should be compact in height."));
    expectEqual(titleFontSize,
                rowFontSize,
                QStringLiteral("Normal-height Transaction classification titles and row labels should use the same size."));
    expectEqual(titleCountFontSize,
                titleFontSize,
                QStringLiteral("Normal-height Transaction classification count should use the title font size."));
    expectEqual(continuationCountFontSize,
                rowFontSize,
                QStringLiteral("Normal-height Transaction continuation-row count should use the row font size."));
    expect(nodeBadge.right() < nodeIdRect.left() && nodeIdRect.right() < reqBadge.left(),
           QStringLiteral("Transaction node badge, node ID, and channel badge should be laid out left-to-right without overlap."));
    expect(widget.testLaneNodeBadgeRect(1).isNull()
               && widget.testLaneNodeIdRect(1).isNull()
               && widget.testLaneChannelBadgeRect(1).isNull(),
           QStringLiteral("Transaction should only display node/channel classification badges on the first row of a group."));

    expect(transactionImageHasColorNear(widget, nodeBadge.center(), 8, [](const QColor& pixel) {
               return pixel.red() >= 35 && pixel.red() <= 60
                   && pixel.green() >= 115 && pixel.green() <= 145
                   && pixel.blue() >= 220 && pixel.blue() <= 245;
           }),
           QStringLiteral("Transaction node-type badge should paint the existing RN-F badge color."));
    expect(transactionImageHasColorNear(widget, nodeIdRect.center(), 6, [](const QColor& pixel) {
               return pixel.red() >= 35 && pixel.red() <= 80
                   && pixel.green() >= 45 && pixel.green() <= 90
                   && pixel.blue() >= 55 && pixel.blue() <= 105;
           }),
           QStringLiteral("Transaction node ID should be drawn as dark text next to the node-type badge."));
    expect(transactionImageHasColorNear(widget, reqBadge.center(), 8, [](const QColor& pixel) {
               return pixel.red() >= 230 && pixel.green() >= 150 && pixel.green() <= 185 && pixel.blue() <= 85;
           }),
           QStringLiteral("Transaction REQ channel badge should paint the trace-table badge color."));
    expect(transactionImageHasColorNear(widget, snpBadge.center(), 8, [](const QColor& pixel) {
               return pixel.red() >= 220 && pixel.green() >= 100 && pixel.green() <= 135 && pixel.blue() >= 95 && pixel.blue() <= 125;
           }),
           QStringLiteral("Transaction SNP channel badge should paint the trace-table badge color."));

    widget.testSetRowHeight(12);
    QApplication::processEvents();
    const QVariantMap compactLayout = widget.testLayoutState();
    const QRect compactNodeBadge = widget.testLaneNodeBadgeRect(0);
    const QRect compactNodeId = widget.testLaneNodeIdRect(0);
    const QRect compactReqBadge = widget.testLaneChannelBadgeRect(0);
    const QRect compactTitleRect = widget.testLaneHeaderTitleRect(0);
    const QRect compactTitleCountRect = widget.testLaneHeaderCountRect(0);
    const QRect compactContinuationCountRect = widget.testLaneHeaderCountRect(1);
    const QRect compactSpanRect = widget.testSpanVisualRect(0);
    expectEqual(compactNodeBadge.height(),
                16,
                QStringLiteral("Classification title badges should keep normal size in compact Transaction rows."));
    expectEqual(compactTitleRect.height(),
                32,
                QStringLiteral("Classification title row should keep its full left-column background height in compact mode."));
    expectEqual(widget.testLaneHeaderTitleFontPixelSize(),
                titleFontSize,
                QStringLiteral("Classification title font size should not shrink with row height."));
    expectEqual(widget.testLaneHeaderCountFontPixelSize(0),
                titleCountFontSize,
                QStringLiteral("Classification first-row transaction count font size should not shrink with row height."));
    expect(widget.testLaneHeaderRowFontPixelSize() < rowFontSize,
           QStringLiteral("Non-title lane row fonts should still shrink with compact row height."));
    expect(widget.testLaneHeaderCountFontPixelSize(1) < continuationCountFontSize,
           QStringLiteral("Continuation-row transaction count font should still shrink with compact row height."));
    expect(compactNodeBadge.height() > compactLayout.value(QStringLiteral("rowHeight")).toInt(),
           QStringLiteral("Compact classification title badges should be allowed to overlap following rows."));
    expect(compactTitleRect.height() > compactLayout.value(QStringLiteral("rowHeight")).toInt(),
           QStringLiteral("Compact classification title background should cover following compact rows in the left column."));
    expectEqual(compactTitleCountRect.height(),
                compactTitleRect.height(),
                QStringLiteral("Classification first-row transaction count should use the unchanged title background height."));
    expect(compactTitleRect.contains(compactContinuationCountRect.center()),
           QStringLiteral("Continuation-row count geometry should sit behind the first-row title cover in compact mode."));
    expectEqual(compactSpanRect.height(),
                compactLayout.value(QStringLiteral("rowHeight")).toInt(),
                QStringLiteral("Compact classification title height should not lock transaction rectangle height on the right."));
    expect(compactNodeBadge.right() < compactNodeId.left() && compactNodeId.right() < compactReqBadge.left(),
           QStringLiteral("Compact classification title badges, node ID, and channel badge should remain ordered left-to-right."));
    const QPoint coveredLaneHeaderPoint(compactTitleRect.left() + 12,
                                        compactTitleRect.top()
                                            + compactLayout.value(QStringLiteral("rowHeight")).toInt()
                                            + compactLayout.value(QStringLiteral("rowHeight")).toInt() / 2);
    expect(transactionImageHasColorNear(widget, coveredLaneHeaderPoint, 3, [](const QColor& pixel) {
               return pixel.red() >= 240 && pixel.red() <= 252
                   && pixel.green() >= 244 && pixel.green() <= 252
                   && pixel.blue() >= 248 && pixel.blue() <= 255;
           }),
           QStringLiteral("Compact rows under a classification title should hide beneath the first row's left-column background."));
}

void testTransactionWidgetCoordinateTransformsAndScrolling()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectSyntheticSpans(12, 36);
    QApplication::processEvents();

    const QVariantMap initialLayout = widget.testLayoutState();
    expect(initialLayout.value(QStringLiteral("visibleLaneCapacity")).toInt() < 12,
           QStringLiteral("The coordinate test fixture should require vertical scrolling."));
    expect(widget.testSpanCenterPoint(8) == QPoint(),
           QStringLiteral("A span below the initial visible stack-row range should not have a visible center."));

    widget.testSetVerticalScroll(6);
    QApplication::processEvents();
    const QVariantMap scrolledLayout = widget.testLayoutState();
    expectEqual(scrolledLayout.value(QStringLiteral("verticalScrollOffset")).toInt(),
                6,
                QStringLiteral("Transaction vertical scrollbar should update the stack-row offset."));
    const QPoint lowerLaneCenter = widget.testSpanCenterPoint(8);
    expect(lowerLaneCenter != QPoint(),
           QStringLiteral("Vertical scrolling should make lower transaction stack rows visible."));
    expectEqual(widget.testSpanIndexAtPoint(lowerLaneCenter),
                8,
                QStringLiteral("Hit testing should find spans after vertical scrolling."));

    const auto fullRange = widget.testVisibleTimestampRange();
    widget.testSetHorizontalZoom(2.0);
    QApplication::processEvents();
    const auto zoomedRange = widget.testVisibleTimestampRange();
    expect(zoomedRange.second - zoomedRange.first < fullRange.second - fullRange.first,
           QStringLiteral("Horizontal zoom should shrink the visible transaction time range."));

    widget.testSetHorizontalScroll(40);
    QApplication::processEvents();
    const auto pannedRange = widget.testVisibleTimestampRange();
    expect(pannedRange.first > zoomedRange.first,
           QStringLiteral("Horizontal scrolling should move the visible transaction time range."));
    expect(pannedRange.second > zoomedRange.second,
           QStringLiteral("Horizontal scrolling should move both visible range edges."));
}

void testTransactionWidgetUsesDensePaintForLargeView()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectSyntheticSpans(4, 2500);
    QApplication::processEvents();

    expect(widget.testUsesDensePaint(),
           QStringLiteral("Large transaction span sets should use dense rendering when zoomed out."));
    expect(widget.testLayoutState().value(QStringLiteral("densePaint")).toBool(),
           QStringLiteral("Dense paint state should be reported through the test layout state."));

    const QPoint firstSpanCenter = widget.testSpanCenterPoint(0);
    expect(firstSpanCenter != QPoint(),
           QStringLiteral("Dense render fixture should still expose visible span geometry."));
    expect(transactionImageHasNonWhiteNear(widget, firstSpanCenter, 5),
           QStringLiteral("Dense transaction render should paint visible tick marks for spans."));
    const QColor reqEdge(widget.testSpanEdgeColorName(0));
    expect(transactionImageHasColorNear(widget, firstSpanCenter, 6, [reqEdge](const QColor& pixel) {
               return pixel.alpha() > 220
                   && std::abs(pixel.red() - reqEdge.red()) <= 3
                   && std::abs(pixel.green() - reqEdge.green()) <= 3
                   && std::abs(pixel.blue() - reqEdge.blue()) <= 3;
           }),
           QStringLiteral("Dense transaction render should use the span edge color when spans collapse to pixels."));
}

void testTransactionWidgetDensePaintPreservesSparseRectangles()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectMixedDensitySyntheticSpans();
    QApplication::processEvents();

    expect(widget.testUsesDensePaint(),
           QStringLiteral("Mixed-density fixture should still enter dense paint for the full viewport."));

    const QPoint congestedCenter = widget.testSpanCenterPoint(0);
    expect(congestedCenter != QPoint(),
           QStringLiteral("Mixed-density fixture should expose a congested span center."));
    const QColor reqEdge(widget.testSpanEdgeColorName(0));
    expect(transactionImageHasColorNear(widget, congestedCenter, 6, [reqEdge](const QColor& pixel) {
               return pixel.alpha() > 220
                   && std::abs(pixel.red() - reqEdge.red()) <= 3
                   && std::abs(pixel.green() - reqEdge.green()) <= 3
                   && std::abs(pixel.blue() - reqEdge.blue()) <= 3;
           }),
           QStringLiteral("Congested local transaction columns should collapse to the edge color."));

    const QRect sparseRect = widget.testSpanVisualRect(2401);
    expect(sparseRect.isValid(),
           QStringLiteral("Mixed-density fixture should expose a sparse rectangle."));
    expect(transactionImageHasColorNear(widget, sparseRect.center(), 8, [](const QColor& pixel) {
               return pixel.red() >= 238 && pixel.green() >= 218 && pixel.blue() >= 190
                   && pixel.red() > pixel.blue()
                   && pixel != QColor(Qt::white);
           }),
           QStringLiteral("Sparse transaction rectangles should keep the normal pale fill inside dense viewports."));

    const QRect narrowSparseRect = widget.testSpanVisualRect(2402);
    expect(narrowSparseRect.isValid() && narrowSparseRect.width() == 5,
           QStringLiteral("Mixed-density fixture should expose an origin-style minimum-width sparse rectangle."));
    expect(transactionImageHasColorNear(widget, narrowSparseRect.center(), 4, [](const QColor& pixel) {
               return pixel.red() >= 238 && pixel.green() >= 218 && pixel.blue() >= 190
                   && pixel.red() > pixel.blue()
                   && pixel != QColor(Qt::white);
           }),
           QStringLiteral("Minimum-width sparse transaction rectangles should not disappear in dense viewports."));

    const QRect mediumCongestedRect = widget.testSpanVisualRect(2404);
    expect(mediumCongestedRect.isValid()
               && mediumCongestedRect.width() >= 8
               && mediumCongestedRect.width() < 32,
           QStringLiteral("Mixed-density fixture should expose a medium-width congested rectangle."));
    expect(transactionImageHasColorNear(widget, mediumCongestedRect.center(), 5, [](const QColor& pixel) {
               return pixel.red() >= 238 && pixel.green() >= 218 && pixel.blue() >= 190
                   && pixel.red() > pixel.blue()
                   && pixel != QColor(Qt::white);
           }),
           QStringLiteral("Medium-width transaction rectangles should not disappear inside dense congestion."));

    const QRect partlyCongestedLongRect = widget.testSpanVisualRect(2403);
    expect(partlyCongestedLongRect.isValid() && partlyCongestedLongRect.width() >= 32,
           QStringLiteral("Mixed-density fixture should expose a long rectangle surrounded by congestion."));
    expect(transactionImageHasColorNear(widget, partlyCongestedLongRect.center(), 8, [](const QColor& pixel) {
               return pixel.red() >= 238 && pixel.green() >= 218 && pixel.blue() >= 190
                   && pixel.red() > pixel.blue()
                   && pixel != QColor(Qt::white);
           }),
           QStringLiteral("Long sparse transaction rectangles should survive when only part of their range is congested."));
    const QPoint longRectLeftProbe(partlyCongestedLongRect.left() + 10, partlyCongestedLongRect.center().y());
    const QPoint longRectRightProbe(partlyCongestedLongRect.right() - 10, partlyCongestedLongRect.center().y());
    expect(transactionImageHasColorNear(widget, longRectLeftProbe, 6, [](const QColor& pixel) {
               return pixel.red() >= 238 && pixel.green() >= 218 && pixel.blue() >= 190
                   && pixel.red() > pixel.blue()
                   && pixel != QColor(Qt::white);
           }),
           QStringLiteral("Long transaction rectangles should not have a left-side hole where congestion overlaps."));
    expect(transactionImageHasColorNear(widget, longRectRightProbe, 6, [](const QColor& pixel) {
               return pixel.red() >= 238 && pixel.green() >= 218 && pixel.blue() >= 190
                   && pixel.red() > pixel.blue()
                   && pixel != QColor(Qt::white);
           }),
           QStringLiteral("Long transaction rectangles should not have a right-side hole where congestion overlaps."));
}

void testTransactionWidgetDenseLaneCacheKeepsRectanglesAfterVerticalScroll()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectMixedDensitySyntheticSpans();
    QApplication::processEvents();

    expect(widget.testUsesDensePaint(),
           QStringLiteral("Dense lane-cache scroll fixture should enter dense paint."));

    (void)renderTransactionImage(widget);

    widget.testSetVerticalScroll(1);
    QApplication::processEvents();

    const QRect scrolledRect = widget.testSpanVisualRect(2404);
    expect(scrolledRect.isValid(),
           QStringLiteral("Vertical scroll should bring the previously partial dense lane fully into view."));
    expect(transactionImageHasColorNear(widget, scrolledRect.center(), 5, [](const QColor& pixel) {
               return pixel.red() >= 238 && pixel.green() >= 218 && pixel.blue() >= 190
                   && pixel.red() > pixel.blue()
                   && pixel != QColor(Qt::white);
           }),
           QStringLiteral("Dense per-lane cache should not reuse a row image with missing rectangle overlays after vertical scrolling."));
}

void testTransactionWidgetHighlightFilterGraysNonMatchingSpans()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectOverlappingSyntheticSpans();
    QApplication::processEvents();

    widget.testSetFilterModeByName(QStringLiteral("highlight"));
    widget.testSetTransactionFilter(QStringLiteral("ReadUnique"), QString(), QString(), QStringLiteral("13"));
    QApplication::processEvents();

    expectEqual(widget.testFilterModeName(),
                QStringLiteral("highlight"),
                QStringLiteral("Transaction filter should enter Highlight mode."));
    expectEqual(widget.testFilterMatchCount(),
                1,
                QStringLiteral("Highlight filter should match the one ReadUnique/DBID 13 transaction."));
    expectEqual(widget.testSpanCount(),
                5,
                QStringLiteral("Highlight filter should keep all original transactions displayed."));
    expect(widget.testSpanAt(1).value(QStringLiteral("filterMatch")).toBool(),
           QStringLiteral("The ReadUnique span should be a filter candidate."));
    expect(!widget.testSpanAt(0).value(QStringLiteral("filterMatch")).toBool(),
           QStringLiteral("A non-matching ReadNoSnp span should be marked as filtered out."));

    const QPoint candidateCenter = widget.testSpanCenterPoint(1);
    expect(candidateCenter != QPoint(),
           QStringLiteral("Highlight filter should keep the matching span hit-testable."));
    expectEqual(widget.testSpanIndexAtPoint(candidateCenter),
                1,
                QStringLiteral("Hit testing should prefer candidate rectangles in Highlight mode."));
    expect(!widget.testSpanVisualRect(0).isValid(),
           QStringLiteral("Filtered-out highlight spans should not be interactive foreground rectangles."));

    const QRect candidateRect = widget.testSpanVisualRect(1);
    expect(transactionImageHasColorNear(widget, candidateRect.center(), 8, [](const QColor& pixel) {
               return pixel.red() >= 238 && pixel.green() >= 218 && pixel.blue() >= 190
                   && pixel.red() > pixel.blue()
                   && pixel != QColor(Qt::white);
           }),
           QStringLiteral("Highlight filter should paint matching candidates in their normal channel color."));

    const QImage image = renderTransactionImage(widget);
    const QPoint filteredSample(widget.testPlotRect().left() + 4,
                                widget.testPlotRect().top()
                                    + widget.testLayoutState().value(QStringLiteral("rowHeight")).toInt() / 2);
    const QColor filteredPixel = QColor::fromRgba(image.pixel(qBound(0, filteredSample.x(), image.width() - 1),
                                                             qBound(0, filteredSample.y(), image.height() - 1)));
    expect(filteredPixel.red() >= 180
               && filteredPixel.green() >= 180
               && filteredPixel.blue() >= 180
               && std::abs(filteredPixel.red() - filteredPixel.green()) < 18
               && std::abs(filteredPixel.green() - filteredPixel.blue()) < 18,
           QStringLiteral("Highlight filter should draw filtered-out rectangles as gray underlay."));
}

void testTransactionWidgetFilterModeRearrangesMatchingSpans()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectOverlappingSyntheticSpans();
    QApplication::processEvents();

    widget.testSetTransactionFilter(QStringLiteral("REQ"), QString(), QString(), QString());
    widget.testSetFilterModeByName(QStringLiteral("filter"));
    QApplication::processEvents();

    expectEqual(widget.testFilterModeName(),
                QStringLiteral("filter"),
                QStringLiteral("Transaction filter should enter Filter mode."));
    expectEqual(widget.testFilterMatchCount(),
                3,
                QStringLiteral("Filter mode should match the three synthetic REQ transactions."));
    expectEqual(widget.testUnfilteredSpanCount(),
                5,
                QStringLiteral("Filter mode should retain the original unfiltered span set."));
    expectEqual(widget.testSpanCount(),
                3,
                QStringLiteral("Filter mode should display only matching transactions."));
    expectEqual(widget.testLaneCount(),
                2,
                QStringLiteral("Filter mode should rearrange stacking for the matching overlapping transactions."));
    expectEqual(widget.testLayoutState().value(QStringLiteral("maxStackDepth")).toInt(),
                2,
                QStringLiteral("Overlapping matching reads should still stack into two rows."));

    widget.testSetTransactionFilter(QStringLiteral("WriteBackFull"), QStringLiteral("0x3000"), QStringLiteral("3"), QString());
    QApplication::processEvents();
    expectEqual(widget.testFilterMatchCount(),
                1,
                QStringLiteral("Opcode/address/TxnID filters should combine to one matching transaction."));
    expectEqual(widget.testSpanCount(),
                1,
                QStringLiteral("Filter mode should show only the combined filter candidate."));
    expectEqual(widget.testLaneCount(),
                1,
                QStringLiteral("A single filtered transaction should be rearranged onto one stack row."));

    QVariantMap state = widget.viewState();
    expectEqual(state.value(QStringLiteral("filterMode")).toString(),
                QStringLiteral("filter"),
                QStringLiteral("Filter mode should persist in the Transaction view state."));
    expectEqual(state.value(QStringLiteral("opcodeFilter")).toString(),
                QStringLiteral("WriteBackFull"),
                QStringLiteral("Opcode filter should persist in the Transaction view state."));

    CHIron::Gui::TransactionWidget restored;
    showTransactionForTest(restored);
    restored.testInjectOverlappingSyntheticSpans();
    restored.restoreViewState(state);
    QApplication::processEvents();
    expectEqual(restored.testFilterModeName(),
                QStringLiteral("filter"),
                QStringLiteral("Restored Transaction widget should keep Filter mode."));
    expectEqual(restored.testSpanCount(),
                1,
                QStringLiteral("Restored Transaction widget should reapply the persisted filter."));
}

void testTransactionWidgetFilteringAcceptsDecodedHexAndDecimalValues()
{
    TransactionTraceFixture fixture =
        makeIndexedReadNoSnpTrace(QStringLiteral("transaction_filter_numeric_forms.clogb"), 8);

    CHIron::Gui::CLogBTraceLoadError error;
    std::shared_ptr<CHIron::Gui::TraceSession> session;
    expect(CHIron::Gui::TraceSession::open(fixture.tracePath, session, error),
           error.summary.isEmpty()
               ? QStringLiteral("Transaction numeric-filter fixture should reopen.")
               : error.summary);
    expect(session != nullptr, QStringLiteral("Transaction numeric-filter session should be created."));
    expect(session->isXactionIndexComplete(),
           QStringLiteral("Transaction numeric-filter fixture should reuse its persisted xaction index."));

    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.setTraceSession(session);
    waitForTransactionSpans(widget,
                            fixture.transactionCount,
                            QStringLiteral("Transaction widget should build spans for numeric filter forms."));

    const QStringList opcodeValues = widget.testSpanAt(0).value(QStringLiteral("opcodeValues")).toStringList();
    expect(opcodeValues.contains(QStringLiteral("ReadNoSnp"))
               && opcodeValues.contains(QStringLiteral("0x4"), Qt::CaseInsensitive),
           QStringLiteral("Transaction spans should retain decoded and raw opcode search aliases."));

    widget.testSetFilterModeByName(QStringLiteral("filter"));
    widget.testSetTransactionFilter(QStringLiteral("ReadNoSnp"), QString(), QString(), QString());
    QApplication::processEvents();
    expectEqual(widget.testSpanCount(),
                fixture.transactionCount,
                QStringLiteral("Decoded opcode text should match all ReadNoSnp transactions."));

    widget.testSetTransactionFilter(QStringLiteral("0x4"), QString(), QString(), QString());
    QApplication::processEvents();
    expectEqual(widget.testSpanCount(),
                fixture.transactionCount,
                QStringLiteral("Hexadecimal opcode filters should match raw opcode aliases."));

    widget.testSetTransactionFilter(QStringLiteral("4"), QString(), QString(), QString());
    QApplication::processEvents();
    expectEqual(widget.testSpanCount(),
                fixture.transactionCount,
                QStringLiteral("Decimal opcode filters should match raw opcode aliases."));

    widget.testSetTransactionFilter(QString(), QStringLiteral("4096"), QString(), QString());
    QApplication::processEvents();
    expectEqual(widget.testSpanCount(),
                1,
                QStringLiteral("Decimal address filters should match hexadecimal address labels."));
    expectEqual(widget.testSpanAt(0).value(QStringLiteral("addressLabel")).toString(),
                QStringLiteral("0x000000001000"),
                QStringLiteral("Decimal address filtering should keep the 0x1000 transaction."));

    widget.testSetTransactionFilter(QString(), QString(), QStringLiteral("0x7"), QStringLiteral("7"));
    QApplication::processEvents();
    expectEqual(widget.testSpanCount(),
                1,
                QStringLiteral("Hexadecimal TxnID and decimal DBID filters should combine through numeric matching."));
    expectEqual(widget.testSpanAt(0).value(QStringLiteral("txnIdLabel")).toString(),
                QStringLiteral("7"),
                QStringLiteral("Numeric TxnID/DBID filtering should keep TxnID 7."));
}

void testTransactionWidgetFilterControlsApplyThroughUiInteractions()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectOverlappingSyntheticSpans();
    QApplication::processEvents();

    auto* modeCombo = widget.findChild<QComboBox*>(QStringLiteral("transactionFilterModeCombo"));
    auto* opcodeEdit = widget.findChild<QLineEdit*>(QStringLiteral("transactionOpcodeFilterEdit"));
    auto* addressEdit = widget.findChild<QLineEdit*>(QStringLiteral("transactionAddressFilterEdit"));
    auto* txnIdEdit = widget.findChild<QLineEdit*>(QStringLiteral("transactionTxnIdFilterEdit"));
    auto* dbidEdit = widget.findChild<QLineEdit*>(QStringLiteral("transactionDbidFilterEdit"));
    auto* clearButton = widget.findChild<QPushButton*>(QStringLiteral("transactionFilterClearButton"));
    expect(modeCombo != nullptr
               && opcodeEdit != nullptr
               && addressEdit != nullptr
               && txnIdEdit != nullptr
               && dbidEdit != nullptr
               && clearButton != nullptr,
           QStringLiteral("Transaction filter controls should be discoverable as child widgets."));
    expect(modeCombo->isVisible()
               && opcodeEdit->isVisible()
               && addressEdit->isVisible()
               && txnIdEdit->isVisible()
               && dbidEdit->isVisible()
               && clearButton->isVisible(),
           QStringLiteral("Transaction filter toolbar controls should be visible at the default test width."));

    opcodeEdit->setFocus();
    opcodeEdit->setText(QStringLiteral("ReadUnique"));
    dbidEdit->setFocus();
    dbidEdit->setText(QStringLiteral("13"));
    QApplication::processEvents();

    expectEqual(widget.testFilterModeName(),
                QStringLiteral("highlight"),
                QStringLiteral("Toolbar filter controls should keep the default Highlight mode."));
    expectEqual(widget.testFilterMatchCount(),
                1,
                QStringLiteral("Typing Opcode and DBID in header controls should update filter candidates."));
    expectEqual(widget.testSpanCount(),
                5,
                QStringLiteral("Highlight mode should keep all spans visible after header-control edits."));

    const int filterModeIndex = modeCombo->findData(QStringLiteral("filter"));
    expect(filterModeIndex >= 0,
           QStringLiteral("Transaction filter mode combo should expose a Filter item."));
    modeCombo->setCurrentIndex(filterModeIndex);
    QApplication::processEvents();
    expectEqual(widget.testFilterModeName(),
                QStringLiteral("filter"),
                QStringLiteral("Changing the header mode combo should switch to Filter mode."));
    expectEqual(widget.testSpanCount(),
                1,
                QStringLiteral("Filter mode selected from the combo should hide non-candidate spans."));

    opcodeEdit->clear();
    dbidEdit->clear();
    addressEdit->setText(QStringLiteral("0x3000"));
    txnIdEdit->setText(QStringLiteral("3"));
    QApplication::processEvents();
    expectEqual(widget.testFilterMatchCount(),
                1,
                QStringLiteral("Toolbar Address and TxnID edits should combine into a single candidate."));
    expectEqual(widget.testSpanAt(0).value(QStringLiteral("requestOpcode")).toString(),
                QStringLiteral("REQ WriteBackFull"),
                QStringLiteral("Toolbar controls should select the WriteBackFull transaction by address and TxnID."));

    addressEdit->clear();
    txnIdEdit->clear();
    QApplication::processEvents();
    typeTextIntoLineEdit(*addressEdit, QStringLiteral("0x2000"));
    expectEqual(addressEdit->text(),
                QStringLiteral("0x2000"),
                QStringLiteral("Typing hexadecimal Address text should preserve the 0x prefix in the toolbar edit."));
    expectEqual(widget.testFilterState().value(QStringLiteral("address")).toString(),
                QStringLiteral("0x2000"),
                QStringLiteral("Typed hexadecimal Address text should be stored as the active Transaction filter."));
    typeTextIntoLineEdit(*txnIdEdit, QStringLiteral("0x2"));
    typeTextIntoLineEdit(*dbidEdit, QStringLiteral("0xd"));
    QApplication::processEvents();
    expectEqual(txnIdEdit->text(),
                QStringLiteral("0x2"),
                QStringLiteral("Typing hexadecimal TxnID text should preserve the 0x prefix in the toolbar edit."));
    expectEqual(dbidEdit->text(),
                QStringLiteral("0xd"),
                QStringLiteral("Typing hexadecimal DBID text should preserve the 0x prefix in the toolbar edit."));
    expectEqual(widget.testFilterMatchCount(),
                1,
                QStringLiteral("Typed hexadecimal Address, TxnID, and DBID filters should remain active together."));
    expectEqual(widget.testSpanAt(0).value(QStringLiteral("requestOpcode")).toString(),
                QStringLiteral("REQ ReadUnique"),
                QStringLiteral("Typed hexadecimal toolbar filters should match the ReadUnique transaction."));

    clearButton->click();
    QApplication::processEvents();
    const QVariantMap clearedState = widget.testFilterState();
    expect(!clearedState.value(QStringLiteral("active")).toBool(),
           QStringLiteral("Transaction filter toolbar Clear button should remove active criteria."));
    expectEqual(widget.testSpanCount(),
                5,
                QStringLiteral("Transaction filter toolbar Clear button should restore all spans."));
}

void testTransactionWidgetContextFilterActionsApplyModeAndField()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectOverlappingSyntheticSpans();
    QApplication::processEvents();

    expect(widget.testInvokeContextAction(1, QStringLiteral("filterOpcode")),
           QStringLiteral("Transaction context menu should offer Filter by Opcode."));
    QApplication::processEvents();
    expectEqual(widget.testFilterModeName(),
                QStringLiteral("filter"),
                QStringLiteral("Filter-by-Opcode context action should switch to Filter mode."));
    expectEqual(widget.testFilterMatchCount(),
                1,
                QStringLiteral("Filter-by-Opcode context action should match only the clicked ReadUnique span."));
    expectEqual(widget.testSpanCount(),
                1,
                QStringLiteral("Filter-by-Opcode context action should hide non-matching spans."));
    expectEqual(widget.testSpanAt(0).value(QStringLiteral("requestOpcode")).toString(),
                QStringLiteral("REQ ReadUnique"),
                QStringLiteral("Filter-by-Opcode context action should keep the clicked opcode visible."));

    expect(widget.testInvokeContextAction(0, QStringLiteral("clearFilter")),
           QStringLiteral("Transaction context menu should offer clearing the active filter."));
    QApplication::processEvents();
    expectEqual(widget.testSpanCount(),
                5,
                QStringLiteral("Clearing the context filter should restore all spans."));

    expect(widget.testInvokeContextAction(0, QStringLiteral("highlightAddress")),
           QStringLiteral("Transaction context menu should offer Highlight by Address."));
    QApplication::processEvents();
    QVariantMap state = widget.testFilterState();
    expectEqual(widget.testFilterModeName(),
                QStringLiteral("highlight"),
                QStringLiteral("Highlight-by-Address context action should switch to Highlight mode."));
    expectEqual(state.value(QStringLiteral("address")).toString(),
                QStringLiteral("0x000000001000"),
                QStringLiteral("Highlight-by-Address should load the clicked span address into the filter."));
    expectEqual(widget.testFilterMatchCount(),
                1,
                QStringLiteral("Highlight-by-Address context action should match one synthetic span."));
    expectEqual(widget.testSpanCount(),
                5,
                QStringLiteral("Highlight-by-Address context action should keep all spans displayed."));

    expect(widget.testInvokeContextAction(1, QStringLiteral("filterDbid")),
           QStringLiteral("Transaction context menu should offer Filter by DBID when the span has a DBID."));
    QApplication::processEvents();
    state = widget.testFilterState();
    expectEqual(widget.testFilterModeName(),
                QStringLiteral("filter"),
                QStringLiteral("Filter-by-DBID context action should switch to Filter mode."));
    expectEqual(state.value(QStringLiteral("dbid")).toString(),
                QStringLiteral("13"),
                QStringLiteral("Filter-by-DBID should load the clicked span DBID into the filter."));
    expectEqual(widget.testSpanCount(),
                1,
                QStringLiteral("Filter-by-DBID context action should hide non-matching spans."));
}

void testTransactionWidgetLargeFilterRunsInBackground()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectSyntheticSpans(16, 20000);
    QApplication::processEvents();

    widget.testSetFilterModeByName(QStringLiteral("filter"));
    widget.testSetTransactionFilter(QString(), QString(), QStringLiteral("7"), QString());

    QVariantMap state = widget.testFilterState();
    expect(state.value(QStringLiteral("filtering")).toBool(),
           QStringLiteral("Large Transaction filters should start in the background."));
    expectEqual(state.value(QStringLiteral("unfilteredSpanCount")).toInt(),
                20000,
                QStringLiteral("Background filtering should retain the unfiltered large span set."));
    expect(widget.testStatusText().startsWith(QStringLiteral("Filtering transactions")),
           QStringLiteral("Background filtering should report real-time progress in the status text."));

    QElapsedTimer waitTimer;
    waitTimer.start();
    while (widget.testFilterState().value(QStringLiteral("filtering")).toBool()
           && waitTimer.elapsed() < 5000) {
        QApplication::processEvents();
        QThread::msleep(5);
    }
    QApplication::processEvents();

    state = widget.testFilterState();
    expect(!state.value(QStringLiteral("filtering")).toBool(),
           QStringLiteral("Large background Transaction filter should complete without blocking the event loop."));
    expectEqual(widget.testFilterMatchCount(),
                1250,
                QStringLiteral("TxnID filter should match the expected one-sixteenth of synthetic spans."));
    expectEqual(widget.testSpanCount(),
                1250,
                QStringLiteral("Filter mode should display only the large-trace background candidates."));
    expectEqual(widget.testFilterModeName(),
                QStringLiteral("filter"),
                QStringLiteral("Background filtering should keep the requested Filter mode."));
    expectEqual(state.value(QStringLiteral("txnId")).toString(),
                QStringLiteral("7"),
                QStringLiteral("Background filtering should keep the requested TxnID criterion."));
}

void testTransactionWidgetClickSelectsSpanAndSnapsCursor()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectSyntheticSpans(4, 12);
    QApplication::processEvents();

    const QPoint spanCenter = widget.testSpanCenterPoint(5);
    expect(spanCenter != QPoint(),
           QStringLiteral("Click-selection fixture span should be visible."));

    sendTransactionMouse(widget,
                         QEvent::MouseButtonPress,
                         spanCenter,
                         Qt::LeftButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    sendTransactionMouse(widget,
                         QEvent::MouseButtonRelease,
                         spanCenter,
                         Qt::LeftButton,
                         Qt::NoButton,
                         Qt::NoModifier);

    expectEqual(widget.testSelectedSpanIndex(),
                5,
                QStringLiteral("Clicking a transaction bar should select that span."));
    expectEqual(widget.testSelectedLogicalRow(),
                15,
                QStringLiteral("Clicking a transaction bar should select its request row."));
    expect(widget.testHasCursor(),
           QStringLiteral("Clicking a transaction bar should show the transaction cursor."));
    const QVariantMap span = widget.testSpanAt(5);
    const qint64 start = span.value(QStringLiteral("startTimestamp")).toLongLong();
    const qint64 end = span.value(QStringLiteral("endTimestamp")).toLongLong();
    expect(widget.testCursorTimestamp() >= start && widget.testCursorTimestamp() <= end,
           QStringLiteral("Transaction cursor should snap inside the selected span time range."));
}

void testTransactionWidgetHoverDisplaysSummaryCardWithFlitTable()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectSyntheticSpans(3, 9);
    QApplication::processEvents();

    const QPoint spanCenter = widget.testSpanCenterPoint(4);
    expect(spanCenter != QPoint(),
           QStringLiteral("Hover-card fixture span should be visible."));
    sendTransactionMouse(widget,
                         QEvent::MouseMove,
                         spanCenter,
                         Qt::NoButton,
                         Qt::NoButton,
                         Qt::NoModifier);

    expect(!widget.testHoverCardVisible(),
           QStringLiteral("Hover card should not display immediately on mouse movement."));
    expect(widget.testHoverCardDwellActive(),
           QStringLiteral("Hovering a transaction span should arm the dwell timer."));
    expect(waitForCondition([&widget]() {
               QApplication::processEvents();
               return widget.testHoverCardVisible();
           }, 900),
           QStringLiteral("Hover card should display after the mouse rests on a transaction span."));
    expect(widget.testHoverCardVisible(),
           QStringLiteral("Hovering a transaction span should display the summary card."));
    const QRect cardRect = widget.testHoverCardRect();
    expect(cardRect.isValid() && widget.rect().contains(cardRect.center()),
           QStringLiteral("Transaction hover card should be positioned inside the widget."));
    expect(widget.testHoverCardTitle().contains(QStringLiteral("RN 1"))
               && widget.testHoverCardTitle().contains(QStringLiteral("ReadNoSnp")),
           QStringLiteral("Transaction hover card title should summarize node and opcode."));
    expectEqual(widget.testHoverCardRowCount(),
                3,
                QStringLiteral("Transaction hover card should include a table of related flits."));
    expectEqual(widget.testHoverCardCellText(0, 0),
                QStringLiteral("12"),
                QStringLiteral("Hover-card flit table should list the request logical row."));
    expectEqual(widget.testHoverCardCellText(0, 2),
                QStringLiteral("-"),
                QStringLiteral("Synthetic fallback rows should keep unavailable channel fields explicit."));
    expect(widget.testHoverCardCellText(0, 4).contains(QStringLiteral("ReadNoSnp")),
           QStringLiteral("Hover-card flit table should include the request opcode."));

    sendTransactionMouse(widget,
                         QEvent::MouseMove,
                         spanCenter + QPoint(1, 0),
                         Qt::NoButton,
                         Qt::NoButton,
                         Qt::NoModifier);
    expect(widget.testHoverCardVisible(),
           QStringLiteral("Minor movement within the hovered span should keep the card visible."));
    expect(widget.testHoverCardRect() == cardRect,
           QStringLiteral("Visible hover card should not trace small mouse movement."));

    sendTransactionMouse(widget,
                         QEvent::MouseMove,
                         widget.testPlotRect().bottomRight() + QPoint(-2, -2),
                         Qt::NoButton,
                         Qt::NoButton,
                         Qt::NoModifier);
    expect(!widget.testHoverCardVisible(),
           QStringLiteral("Moving away from transaction spans should hide the hover card."));
}

void testTransactionWidgetHoverCardsToolbarToggle()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectSyntheticSpans(3, 9);
    QApplication::processEvents();

    auto* checkBox = widget.findChild<QCheckBox*>(QStringLiteral("transactionHoverCardsCheckBox"));
    expect(checkBox != nullptr && checkBox->isChecked(),
           QStringLiteral("Transaction toolbar should expose a checked hover-card checkbox by default."));
    const QRect checkRect = widget.testHoverCardsCheckBoxRect();
    expect(checkRect.isValid(),
           QStringLiteral("Hover-card checkbox should be laid out in the Transaction toolbar."));

    const QPoint spanCenter = widget.testSpanCenterPoint(4);
    expect(spanCenter != QPoint(),
           QStringLiteral("Hover-card toggle fixture span should be visible."));
    sendTransactionMouse(widget,
                         QEvent::MouseMove,
                         spanCenter,
                         Qt::NoButton,
                         Qt::NoButton,
                         Qt::NoModifier);
    expect(widget.testHoverCardDwellActive(),
           QStringLiteral("Hover-card dwell timer should arm while the toolbar toggle is enabled."));

    checkBox->click();
    QApplication::processEvents();
    expect(!widget.testShowHoverCards() && !checkBox->isChecked(),
           QStringLiteral("Clicking the hover-card checkbox should disable hover summary cards."));
    expect(!widget.testHoverCardDwellActive() && !widget.testHoverCardVisible(),
           QStringLiteral("Disabling hover summary cards should cancel pending and visible cards."));

    sendTransactionMouse(widget,
                         QEvent::MouseMove,
                         spanCenter + QPoint(2, 0),
                         Qt::NoButton,
                         Qt::NoButton,
                         Qt::NoModifier);
    QApplication::processEvents();
    expect(!widget.testHoverCardDwellActive() && !widget.testHoverCardVisible(),
           QStringLiteral("Disabled hover summary cards should not arm on mouse movement."));

    QVariantMap state = widget.viewState();
    expect(!state.value(QStringLiteral("showHoverCards")).toBool(),
           QStringLiteral("Hover-card visibility should persist in Transaction view state."));

    CHIron::Gui::TransactionWidget restored;
    showTransactionForTest(restored);
    restored.testInjectSyntheticSpans(3, 9);
    restored.restoreViewState(state);
    QApplication::processEvents();
    expect(!restored.testShowHoverCards(),
           QStringLiteral("Restoring Transaction view state should keep hover summary cards disabled."));

    const QPoint restoredCenter = restored.testSpanCenterPoint(4);
    sendTransactionMouse(restored,
                         QEvent::MouseMove,
                         restoredCenter,
                         Qt::NoButton,
                         Qt::NoButton,
                         Qt::NoModifier);
    QApplication::processEvents();
    expect(!restored.testHoverCardDwellActive() && !restored.testHoverCardVisible(),
           QStringLiteral("Restored disabled hover summary cards should stay inactive."));
}

void testTransactionWidgetDoubleClickEnterAndContextNavigation()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectSyntheticSpans(3, 9);
    QApplication::processEvents();

    int activatedRow = -1;
    QObject::connect(&widget,
                     &CHIron::Gui::TransactionWidget::logicalRowActivated,
                     &widget,
                     [&activatedRow](const int logicalRow) {
                         activatedRow = logicalRow;
                     });

    const QPoint spanCenter = widget.testSpanCenterPoint(4);
    expect(spanCenter != QPoint(),
           QStringLiteral("Activation fixture span should be visible."));
    sendTransactionMouse(widget,
                         QEvent::MouseButtonDblClick,
                         spanCenter,
                         Qt::LeftButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    expectEqual(activatedRow,
                12,
                QStringLiteral("Double-clicking a span should activate its request row."));

    activatedRow = -1;
    sendTransactionKey(widget, Qt::Key_Return);
    expectEqual(activatedRow,
                12,
                QStringLiteral("Enter should activate the selected span request row."));

    activatedRow = -1;
    expect(widget.testInvokeContextAction(4, QStringLiteral("completion")),
           QStringLiteral("Completion context action should be available for transaction spans."));
    expectEqual(activatedRow,
                14,
                QStringLiteral("Completion context action should activate the completion row."));
    expectEqual(widget.testSelectedLogicalRow(),
                14,
                QStringLiteral("Completion context action should select the completion row."));

    const QString relatedRows = widget.testRelatedRowsTextForSpan(4);
    expect(relatedRows.contains(QStringLiteral("Rows: 12, 13, 14")),
           QStringLiteral("Related rows context text should list the span logical rows."));
    const QString debugText = widget.testDebugTextForSpan(4);
    expect(debugText.contains(QStringLiteral("Ordinal: 4"))
               && debugText.contains(QStringLiteral("Opcode: ReadNoSnp")),
           QStringLiteral("Debug context text should describe the selected transaction span."));
    expect(widget.testInvokeContextAction(4, QStringLiteral("relatedRows")),
           QStringLiteral("Related-rows context test action should produce text."));
    expect(widget.testInvokeContextAction(4, QStringLiteral("debug")),
           QStringLiteral("Debug context test action should produce text."));
}

void testTransactionWidgetSelectedOpcodeTagsLayoutAndJump()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectOpcodeTagSyntheticSpans();
    QApplication::processEvents();

    const QPoint spanCenter = widget.testSpanCenterPoint(0);
    expect(spanCenter != QPoint(),
           QStringLiteral("Opcode-tag fixture span should be visible."));
    sendTransactionMouse(widget,
                         QEvent::MouseButtonPress,
                         spanCenter,
                         Qt::LeftButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    sendTransactionMouse(widget,
                         QEvent::MouseButtonRelease,
                         spanCenter,
                         Qt::LeftButton,
                         Qt::NoButton,
                         Qt::NoModifier);
    QApplication::processEvents();

    expectEqual(widget.testSelectedSpanIndex(),
                0,
                QStringLiteral("Opcode tags should be produced for the selected span."));
    expectGreater(static_cast<double>(widget.testSelectedOpcodeTagCount()),
                  0.0,
                  QStringLiteral("Selected opcode tags should include visible request or related flits."));

    QRect previousTagRect;
    for (int index = 0; index < widget.testSelectedOpcodeTagCount(); ++index) {
        const QVariantMap tag = widget.testSelectedOpcodeTagAt(index);
        const QRect tagRect = tag.value(QStringLiteral("rect")).toRect();
        const int timestampX = tag.value(QStringLiteral("timestampX")).toInt();
        expect(tagRect.isValid(), QStringLiteral("Selected opcode tag should expose a valid tag rectangle."));
        expect(tagRect.contains(QPoint(timestampX, tagRect.center().y())),
               QStringLiteral("Opcode tag should keep the flit timestamp attached to the pill."));
        if (previousTagRect.isValid()) {
            expect(!previousTagRect.intersects(tagRect),
                   QStringLiteral("Nearby opcode tags should avoid overlapping each other."));
        }
        previousTagRect = tagRect;

        const QRect triangleBounds = tag.value(QStringLiteral("triangleBounds")).toRect();
        expect(tag.value(QStringLiteral("triangleVisible")).toBool(),
               QStringLiteral("Opcode tag should draw a triangle arrow when there is room."));
        expect(triangleBounds.isValid() && triangleBounds.center().x() >= tagRect.left()
                   && triangleBounds.center().x() <= tagRect.right(),
               QStringLiteral("Opcode tag triangle should attach to the tag edge near the timestamp."));
        const bool bottom = tag.value(QStringLiteral("bottomPlacement")).toBool();
        const int tipY = tag.value(QStringLiteral("triangleY2")).toInt();
        if (bottom) {
            expect(tipY < tagRect.top(),
                   QStringLiteral("Bottom opcode tags should use an upward triangle toward the span."));
        } else {
            expect(tipY > tagRect.bottom(),
                   QStringLiteral("Top opcode tags should use a downward triangle toward the span."));
        }
        const QRect inlineTextRect = tag.value(QStringLiteral("inlineTextRect")).toRect();
        expect(!inlineTextRect.isValid() || !triangleBounds.adjusted(-1, -1, 1, 1).intersects(inlineTextRect),
               QStringLiteral("Opcode tag triangles should not overlap the inline span label."));
    }

    expectEqual(widget.testSelectedOpcodeTagAt(0).value(QStringLiteral("logicalRow")).toULongLong(),
                static_cast<qulonglong>(0),
                QStringLiteral("The selected REQ request row should get the first opcode tag."));
    expectEqual(widget.testSelectedOpcodeTagAt(0).value(QStringLiteral("color")).toString(),
                CHIron::Gui::ChannelAccent(CHIron::Gui::FlitChannel::Req).name(QColor::HexRgb).toUpper(),
                QStringLiteral("REQ request-row opcode tags should use the trace-table REQ accent color."));
    bool sawRspTag = false;
    bool sawDatTag = false;
    for (int index = 0; index < widget.testSelectedOpcodeTagCount(); ++index) {
        const QVariantMap tag = widget.testSelectedOpcodeTagAt(index);
        if (tag.value(QStringLiteral("logicalRow")).toULongLong() == static_cast<qulonglong>(2)) {
            sawRspTag = true;
            expectEqual(tag.value(QStringLiteral("color")).toString(),
                        QColor(QStringLiteral("#5AB9D6")).name(QColor::HexRgb).toUpper(),
                        QStringLiteral("RSP opcode tags should use the trace-table channel accent color."));
        }
        if (tag.value(QStringLiteral("logicalRow")).toULongLong() == static_cast<qulonglong>(4)) {
            sawDatTag = true;
            expectEqual(tag.value(QStringLiteral("color")).toString(),
                        QColor(QStringLiteral("#7BC96F")).name(QColor::HexRgb).toUpper(),
                        QStringLiteral("DAT opcode tags should use the trace-table channel accent color."));
        }
    }
    expect(sawRspTag && sawDatTag,
           QStringLiteral("The default opcode-tag fixture should keep the first RSP and DAT tags visible."));

    int activatedRow = -1;
    QObject::connect(&widget,
                     &CHIron::Gui::TransactionWidget::logicalRowActivated,
                     &widget,
                     [&activatedRow](const int logicalRow) {
                         activatedRow = logicalRow;
                     });
    QRect tagRect = widget.testSelectedOpcodeTagAt(0).value(QStringLiteral("rect")).toRect();
    sendTransactionMouse(widget,
                         QEvent::MouseButtonDblClick,
                         tagRect.center(),
                         Qt::LeftButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    expectEqual(activatedRow,
                0,
                QStringLiteral("Double-clicking the request opcode tag should activate the request flit row."));
    expectEqual(widget.testSelectedLogicalRow(),
                0,
                QStringLiteral("Double-clicking the request opcode tag should select the request flit row."));
    expectEqual(static_cast<int>(widget.testCursorTimestamp()),
                100,
                QStringLiteral("Double-clicking the request opcode tag should stick the cursor to the request tag timestamp."));

    activatedRow = -1;
    tagRect = QRect();
    for (int index = 0; index < widget.testSelectedOpcodeTagCount(); ++index) {
        if (widget.testSelectedOpcodeTagAt(index).value(QStringLiteral("logicalRow")).toULongLong()
            == static_cast<qulonglong>(4)) {
            tagRect = widget.testSelectedOpcodeTagAt(index).value(QStringLiteral("rect")).toRect();
            break;
        }
    }
    expect(tagRect.isValid(),
           QStringLiteral("The DAT opcode tag should be available for activation."));
    widget.testSetHorizontalZoom(4.0);
    widget.testSetHorizontalScroll(0);
    QApplication::processEvents();
    QVariantMap zoomedDatTag;
    for (int index = 0; index < widget.testSelectedOpcodeTagCount(); ++index) {
        const QVariantMap candidate = widget.testSelectedOpcodeTagAt(index);
        if (candidate.value(QStringLiteral("logicalRow")).toULongLong() == static_cast<qulonglong>(4)) {
            zoomedDatTag = candidate;
            tagRect = candidate.value(QStringLiteral("rect")).toRect();
            break;
        }
    }
    expect(tagRect.isValid(),
           QStringLiteral("The DAT opcode tag should remain hittable under a zoomed viewport."));
    const bool zoomedTagBottom = zoomedDatTag.value(QStringLiteral("bottomPlacement")).toBool();
    const QPoint enlargedHitPoint(tagRect.center().x(),
                                  zoomedTagBottom ? tagRect.top() - 2 : tagRect.bottom() + 2);
    expect(!tagRect.contains(enlargedHitPoint) && widget.testPlotRect().contains(enlargedHitPoint),
           QStringLiteral("The opcode-tag regression point should land just outside the painted tag."));
    activatedRow = -1;
    sendTransactionMouse(widget,
                         QEvent::MouseButtonPress,
                         enlargedHitPoint,
                         Qt::LeftButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    sendTransactionMouse(widget,
                         QEvent::MouseButtonRelease,
                         enlargedHitPoint,
                         Qt::LeftButton,
                         Qt::NoButton,
                         Qt::NoModifier);
    expectEqual(activatedRow,
                -1,
                QStringLiteral("Single-clicking the expanded opcode-tag hit area should not activate the flit row."));
    expectEqual(widget.testSelectedLogicalRow(),
                4,
                QStringLiteral("The expanded opcode-tag hit area should select the tag instead of the span behind it."));
    expectEqual(static_cast<int>(widget.testCursorTimestamp()),
                118,
                QStringLiteral("The expanded opcode-tag hit area should align the cursor with the tag timestamp."));

    const QVariantMap layoutBeforeZoomedTagDoubleClick = widget.testLayoutState();
    const QMetaObject::Connection syncConnection =
        QObject::connect(&widget,
                         &CHIron::Gui::TransactionWidget::logicalRowActivated,
                         &widget,
                         [&widget](const int logicalRow) {
                             widget.setSelectedLogicalRow(logicalRow);
                         });
    sendTransactionMouse(widget,
                         QEvent::MouseButtonDblClick,
                         enlargedHitPoint,
                         Qt::LeftButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    QObject::disconnect(syncConnection);
    const QVariantMap layoutAfterZoomedTagDoubleClick = widget.testLayoutState();
    expectEqual(activatedRow,
                4,
                QStringLiteral("Double-clicking an opcode tag should activate its exact flit row."));
    expectEqual(widget.testSelectedLogicalRow(),
                4,
                QStringLiteral("Double-clicking an opcode tag should select its exact flit row."));
    expectEqual(static_cast<int>(widget.testCursorTimestamp()),
                118,
                QStringLiteral("Double-clicking an opcode tag should stick the cursor to the exact flit timestamp."));
    expectEqual(layoutAfterZoomedTagDoubleClick.value(QStringLiteral("horizontalScrollOffset")).toULongLong(),
                layoutBeforeZoomedTagDoubleClick.value(QStringLiteral("horizontalScrollOffset")).toULongLong(),
                QStringLiteral("Double-clicking an opcode tag should not move the horizontal transaction view span."));
    expectEqual(layoutAfterZoomedTagDoubleClick.value(QStringLiteral("verticalScrollOffset")).toInt(),
                layoutBeforeZoomedTagDoubleClick.value(QStringLiteral("verticalScrollOffset")).toInt(),
                QStringLiteral("Double-clicking an opcode tag should not move the vertical transaction view span."));

    widget.testSetHorizontalZoom(1.0);
    widget.testSetHorizontalScroll(0);
    widget.setSelectedLogicalRow(0);
    QApplication::processEvents();
    tagRect = QRect();
    for (int index = 0; index < widget.testSelectedOpcodeTagCount(); ++index) {
        if (widget.testSelectedOpcodeTagAt(index).value(QStringLiteral("logicalRow")).toULongLong()
            == static_cast<qulonglong>(4)) {
            tagRect = widget.testSelectedOpcodeTagAt(index).value(QStringLiteral("rect")).toRect();
            break;
        }
    }
    expect(tagRect.isValid(),
           QStringLiteral("The DAT opcode tag should still be available for a real double-click sequence."));
    activatedRow = -1;
    sendTransactionMouse(widget,
                         QEvent::MouseButtonPress,
                         tagRect.center(),
                         Qt::LeftButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    sendTransactionMouse(widget,
                         QEvent::MouseButtonRelease,
                         tagRect.center(),
                         Qt::LeftButton,
                         Qt::NoButton,
                         Qt::NoModifier);
    sendTransactionMouse(widget,
                         QEvent::MouseButtonDblClick,
                         tagRect.center(),
                         Qt::LeftButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    expectEqual(activatedRow,
                4,
                QStringLiteral("A real opcode-tag double-click sequence should still activate the exact flit row."));
    expectEqual(static_cast<int>(widget.testCursorTimestamp()),
                118,
                QStringLiteral("A real opcode-tag double-click sequence should keep the cursor stuck to the tag timestamp."));

    widget.setSelectedLogicalRow(4);
    QApplication::processEvents();
    expectEqual(static_cast<int>(widget.testCursorTimestamp()),
                118,
                QStringLiteral("Main-table sync after opcode-tag activation should preserve the exact tag timestamp."));
    sendTransactionWheel(widget, tagRect.center(), 120, Qt::ControlModifier);
    expectEqual(static_cast<int>(widget.testCursorTimestamp()),
                118,
                QStringLiteral("Ctrl-wheel zoom after opcode-tag activation should keep the cursor on the tag timestamp."));

    QRect zoomedTagRect;
    for (int index = 0; index < widget.testSelectedOpcodeTagCount(); ++index) {
        const QVariantMap tag = widget.testSelectedOpcodeTagAt(index);
        if (tag.value(QStringLiteral("logicalRow")).toULongLong() == static_cast<qulonglong>(4)) {
            zoomedTagRect = tag.value(QStringLiteral("rect")).toRect();
            break;
        }
    }
    expect(zoomedTagRect.isValid(),
           QStringLiteral("The selected opcode tag should remain visible after ctrl-wheel zoom."));
    const QPoint rangeStart(zoomedTagRect.center().x() - 80, widget.testPlotRect().center().y());
    const QPoint rangeEnd(zoomedTagRect.center().x() + 80, widget.testPlotRect().center().y() + 1);
    sendTransactionMouse(widget,
                         QEvent::MouseButtonPress,
                         rangeStart,
                         Qt::LeftButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    sendTransactionMouse(widget,
                         QEvent::MouseMove,
                         rangeEnd,
                         Qt::NoButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    sendTransactionMouse(widget,
                         QEvent::MouseButtonRelease,
                         rangeEnd,
                         Qt::LeftButton,
                         Qt::NoButton,
                         Qt::NoModifier);
    expectEqual(static_cast<int>(widget.testCursorTimestamp()),
                118,
                QStringLiteral("Horizontal range zoom after opcode-tag activation should keep the cursor on the tag timestamp."));
}

void testTransactionWidgetOpcodeTagsSnpRequestTag()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectOpcodeTagSyntheticSpans();
    widget.testConfigureOpcodeTagSyntheticSpan(true, false);
    QApplication::processEvents();

    const QPoint spanCenter = widget.testSpanCenterPoint(0);
    expect(spanCenter != QPoint(),
           QStringLiteral("SNP opcode-tag fixture span should be visible."));
    sendTransactionMouse(widget,
                         QEvent::MouseButtonPress,
                         spanCenter,
                         Qt::LeftButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    sendTransactionMouse(widget,
                         QEvent::MouseButtonRelease,
                         spanCenter,
                         Qt::LeftButton,
                         Qt::NoButton,
                         Qt::NoModifier);
    QApplication::processEvents();

    expectGreater(static_cast<double>(widget.testSelectedOpcodeTagCount()),
                  0.0,
                  QStringLiteral("SNP-origin selected spans should expose visible opcode tags."));
    const QVariantMap requestTag = widget.testSelectedOpcodeTagAt(0);
    expectEqual(requestTag.value(QStringLiteral("logicalRow")).toULongLong(),
                static_cast<qulonglong>(0),
                QStringLiteral("The SNP request tag should point at the request logical row."));
    expectEqual(requestTag.value(QStringLiteral("opcode")).toString(),
                QStringLiteral("SnpMakeInvalid"),
                QStringLiteral("The SNP request tag should show the first SNP opcode without the channel prefix."));
    expectEqual(requestTag.value(QStringLiteral("color")).toString(),
                CHIron::Gui::ChannelAccent(CHIron::Gui::FlitChannel::Snp).name(QColor::HexRgb).toUpper(),
                QStringLiteral("SNP request-row opcode tags should use the trace-table SNP accent color."));
}

void testTransactionWidgetOpcodeTagsSingleClickSelection()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectOpcodeTagSyntheticSpans();
    QApplication::processEvents();

    const QPoint spanCenter = widget.testSpanCenterPoint(0);
    expect(spanCenter != QPoint(),
           QStringLiteral("Opcode-tag selection fixture span should be visible."));
    sendTransactionMouse(widget,
                         QEvent::MouseButtonPress,
                         spanCenter,
                         Qt::LeftButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    sendTransactionMouse(widget,
                         QEvent::MouseButtonRelease,
                         spanCenter,
                         Qt::LeftButton,
                         Qt::NoButton,
                         Qt::NoModifier);
    QApplication::processEvents();
    expectEqual(widget.testSelectedSpanIndex(),
                0,
                QStringLiteral("Selecting an opcode tag starts from a selected transaction rectangle."));

    int activatedRow = -1;
    QObject::connect(&widget,
                     &CHIron::Gui::TransactionWidget::logicalRowActivated,
                     &widget,
                     [&activatedRow](const int logicalRow) {
                         activatedRow = logicalRow;
                     });

    int tagIndex = -1;
    QVariantMap tag;
    for (int index = 0; index < widget.testSelectedOpcodeTagCount(); ++index) {
        const QVariantMap candidate = widget.testSelectedOpcodeTagAt(index);
        if (candidate.value(QStringLiteral("logicalRow")).toULongLong() == static_cast<qulonglong>(4)) {
            tagIndex = index;
            tag = candidate;
            break;
        }
    }
    expect(tagIndex >= 0,
           QStringLiteral("Opcode-tag selection fixture should expose a DAT tag."));

    const QRect tagRect = tag.value(QStringLiteral("rect")).toRect();
    sendTransactionMouse(widget,
                         QEvent::MouseButtonPress,
                         tagRect.center(),
                         Qt::LeftButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    sendTransactionMouse(widget,
                         QEvent::MouseButtonRelease,
                         tagRect.center(),
                         Qt::LeftButton,
                         Qt::NoButton,
                         Qt::NoModifier);
    QApplication::processEvents();

    expectEqual(activatedRow,
                -1,
                QStringLiteral("Single-clicking an opcode tag should not activate a jump."));
    expectEqual(widget.testSelectedSpanIndex(),
                0,
                QStringLiteral("Single-clicking an opcode tag should keep the related rectangle selected."));
    expectEqual(widget.testSelectedLogicalRow(),
                4,
                QStringLiteral("Single-clicking an opcode tag should select the tag flit row."));
    expectEqual(static_cast<int>(widget.testCursorTimestamp()),
                118,
                QStringLiteral("Single-clicking an opcode tag should align the cursor to the tag timestamp."));

    QVariantMap selectedTag;
    for (int index = 0; index < widget.testSelectedOpcodeTagCount(); ++index) {
        const QVariantMap candidate = widget.testSelectedOpcodeTagAt(index);
        if (candidate.value(QStringLiteral("logicalRow")).toULongLong() == static_cast<qulonglong>(4)) {
            selectedTag = candidate;
            continue;
        }
        expect(!candidate.value(QStringLiteral("selected")).toBool(),
               QStringLiteral("Only the clicked opcode tag should be selected."));
    }
    expect(selectedTag.value(QStringLiteral("selected")).toBool(),
           QStringLiteral("The clicked opcode tag should report selected state."));
    expect(selectedTag.value(QStringLiteral("hasBlackEdge")).toBool()
               && selectedTag.value(QStringLiteral("edgeColor")).toString() == QStringLiteral("#000000"),
           QStringLiteral("The selected opcode tag should expose a black edge for tests."));

    QVariantMap requestTag = widget.testSelectedOpcodeTagAt(0);
    sendTransactionMouse(widget,
                         QEvent::MouseButtonPress,
                         requestTag.value(QStringLiteral("rect")).toRect().center(),
                         Qt::LeftButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    sendTransactionMouse(widget,
                         QEvent::MouseButtonRelease,
                         requestTag.value(QStringLiteral("rect")).toRect().center(),
                         Qt::LeftButton,
                         Qt::NoButton,
                         Qt::NoModifier);
    QApplication::processEvents();
    expect(widget.testSelectedOpcodeTagAt(0).value(QStringLiteral("selected")).toBool(),
           QStringLiteral("Selecting another opcode tag should move tag selection."));
    for (int index = 1; index < widget.testSelectedOpcodeTagCount(); ++index) {
        expect(!widget.testSelectedOpcodeTagAt(index).value(QStringLiteral("selected")).toBool(),
               QStringLiteral("Selecting another opcode tag should clear the previous tag selection."));
    }

    sendTransactionMouse(widget,
                         QEvent::MouseButtonPress,
                         spanCenter,
                         Qt::LeftButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    sendTransactionMouse(widget,
                         QEvent::MouseButtonRelease,
                         spanCenter,
                         Qt::LeftButton,
                         Qt::NoButton,
                         Qt::NoModifier);
    QApplication::processEvents();
    expectEqual(widget.testSelectedSpanIndex(),
                0,
                QStringLiteral("Clicking the rectangle body should keep the rectangle selected."));
    expectEqual(widget.testSelectedLogicalRow(),
                0,
                QStringLiteral("Clicking the rectangle body should return selection to the request row."));
    for (int index = 0; index < widget.testSelectedOpcodeTagCount(); ++index) {
        expect(!widget.testSelectedOpcodeTagAt(index).value(QStringLiteral("selected")).toBool(),
               QStringLiteral("Clicking the rectangle body should clear opcode tag selection."));
    }
}

void testTransactionWidgetOpcodeTagsRecoverFullTextWhenZoomed()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectOpcodeTagSyntheticSpans();
    widget.testConfigureOpcodeTagSyntheticSpan(false, false);
    widget.setSelectedLogicalRow(0);
    QApplication::processEvents();

    widget.testSetHorizontalZoom(2.0);
    widget.setSelectedLogicalRow(0);
    widget.testSetHorizontalScroll(0);
    QApplication::processEvents();

    bool foundLongTag = false;
    for (int index = 0; index < widget.testSelectedOpcodeTagCount(); ++index) {
        const QVariantMap tag = widget.testSelectedOpcodeTagAt(index);
        if (tag.value(QStringLiteral("opcode")).toString() != QStringLiteral("CompDBIDRespReallyLongName")) {
            continue;
        }
        foundLongTag = true;
        expectEqual(tag.value(QStringLiteral("text")).toString(),
                    QStringLiteral("CompDBIDRespReallyLongName"),
                    QStringLiteral("A zoomed-wide opcode tag should recover the full opcode text."));
    }
    expect(foundLongTag,
           QStringLiteral("The zoomed opcode-tag fixture should keep the long RSP tag visible."));
}

void testTransactionWidgetOpcodeTagsInterleaveWhenCongested()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectOpcodeTagSyntheticSpans();
    widget.testConfigureOpcodeTagSyntheticSpan(false, true);
    QApplication::processEvents();

    const QPoint spanCenter = widget.testSpanCenterPoint(0);
    expect(spanCenter != QPoint(),
           QStringLiteral("Congested opcode-tag fixture span should be visible."));
    sendTransactionMouse(widget,
                         QEvent::MouseButtonPress,
                         spanCenter,
                         Qt::LeftButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    sendTransactionMouse(widget,
                         QEvent::MouseButtonRelease,
                         spanCenter,
                         Qt::LeftButton,
                         Qt::NoButton,
                         Qt::NoModifier);
    QApplication::processEvents();

    expectGreater(static_cast<double>(widget.testSelectedOpcodeTagCount()),
                  4.0,
                  QStringLiteral("Congested opcode tags should still fit several related flits by using both edges."));
    bool sawTop = false;
    bool sawBottom = false;
    std::vector<QRect> tagRects;
    for (int index = 0; index < widget.testSelectedOpcodeTagCount(); ++index) {
        const QVariantMap tag = widget.testSelectedOpcodeTagAt(index);
        const QRect tagRect = tag.value(QStringLiteral("rect")).toRect();
        expect(tagRect.isValid(),
               QStringLiteral("Congested opcode tags should expose valid rectangles."));
        sawBottom = sawBottom || tag.value(QStringLiteral("bottomPlacement")).toBool();
        sawTop = sawTop || !tag.value(QStringLiteral("bottomPlacement")).toBool();
        for (const QRect& existing : tagRects) {
            expect(!existing.intersects(tagRect),
                   QStringLiteral("Interleaved congested opcode tags should not overlap."));
        }
        tagRects.push_back(tagRect);
    }
    expect(sawTop && sawBottom,
           QStringLiteral("Congested opcode tags should interleave between top and bottom edges when both fit."));
}

void testTransactionWidgetOpcodeTagsElideIndependently()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectOpcodeTagSyntheticSpans();
    widget.testConfigureOpcodeTagSyntheticSpan(false, true);
    widget.setSelectedLogicalRow(0);
    QApplication::processEvents();

    bool sawShortFullTag = false;
    bool sawLongElidedTag = false;
    QSet<QString> visibleTexts;
    for (int index = 0; index < widget.testSelectedOpcodeTagCount(); ++index) {
        const QVariantMap tag = widget.testSelectedOpcodeTagAt(index);
        const QString opcode = tag.value(QStringLiteral("opcode")).toString();
        const QString text = tag.value(QStringLiteral("text")).toString();
        const QRect rect = tag.value(QStringLiteral("rect")).toRect();
        visibleTexts.insert(text);
        if (opcode == QStringLiteral("Data") && text == opcode) {
            sawShortFullTag = true;
        }
        if (opcode == QStringLiteral("CompDataReallyLongResponseForIndependentOpcodeTagElisionAcrossTheEntireVisibleTransactionPlot")
            && text != opcode
            && rect.width() > 0) {
            sawLongElidedTag = true;
        }
    }

    expect(sawShortFullTag,
           QStringLiteral("Short opcode tags should keep full text even when nearby long tags need elision."));
    expect(sawLongElidedTag,
           QStringLiteral("Long congested opcode tags should still be allowed to elide independently."));
    expectGreater(static_cast<double>(visibleTexts.size()),
                  2.0,
                  QStringLiteral("Opcode tag elision should produce per-tag text, not a shared uniform truncation."));
}

void testTransactionWidgetOpcodeTagsToolbarToggle()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectOpcodeTagSyntheticSpans();
    QApplication::processEvents();

    const QPoint spanCenter = widget.testSpanCenterPoint(0);
    expect(spanCenter != QPoint(),
           QStringLiteral("Opcode-tag toggle fixture span should be visible."));
    sendTransactionMouse(widget,
                         QEvent::MouseButtonPress,
                         spanCenter,
                         Qt::LeftButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    sendTransactionMouse(widget,
                         QEvent::MouseButtonRelease,
                         spanCenter,
                         Qt::LeftButton,
                         Qt::NoButton,
                         Qt::NoModifier);
    QApplication::processEvents();
    expectEqual(widget.testSelectedOpcodeTagCount(),
                5,
                QStringLiteral("Opcode tags should be enabled by default."));

    auto* checkBox = widget.findChild<QCheckBox*>(QStringLiteral("transactionOpcodeTagsCheckBox"));
    expect(checkBox != nullptr && checkBox->isChecked(),
           QStringLiteral("Transaction toolbar should expose a checked opcode-tag checkbox by default."));
    const QRect checkRect = widget.testOpcodeTagsCheckBoxRect();
    expect(checkRect.isValid(),
           QStringLiteral("Opcode-tag checkbox should be laid out in the Transaction toolbar."));
    checkBox->click();
    QApplication::processEvents();
    expect(!widget.testShowOpcodeTags() && !checkBox->isChecked(),
           QStringLiteral("Clicking the opcode-tag checkbox should disable opcode tags."));
    expectEqual(widget.testSelectedOpcodeTagCount(),
                0,
                QStringLiteral("Disabled opcode tags should not produce selected-span tag layouts."));

    QVariantMap state = widget.viewState();
    expect(!state.value(QStringLiteral("showOpcodeTags")).toBool(),
           QStringLiteral("Opcode-tag visibility should persist in Transaction view state."));

    CHIron::Gui::TransactionWidget restored;
    showTransactionForTest(restored);
    restored.testInjectOpcodeTagSyntheticSpans();
    restored.restoreViewState(state);
    restored.setSelectedLogicalRow(0);
    QApplication::processEvents();
    expect(!restored.testShowOpcodeTags(),
           QStringLiteral("Restoring Transaction view state should keep opcode tags disabled."));
    expectEqual(restored.testSelectedOpcodeTagCount(),
                0,
                QStringLiteral("Restored disabled opcode tags should stay hidden."));
}

void testTransactionWidgetSelectedOpcodeTagsEdgesLinesAndScaling()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectOpcodeTagSyntheticSpans();
    QApplication::processEvents();

    sendTransactionMouse(widget,
                         QEvent::MouseButtonPress,
                         widget.testSpanCenterPoint(0),
                         Qt::LeftButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    sendTransactionMouse(widget,
                         QEvent::MouseButtonRelease,
                         widget.testSpanCenterPoint(0),
                         Qt::LeftButton,
                         Qt::NoButton,
                         Qt::NoModifier);
    QApplication::processEvents();

    const QRect plot = widget.testPlotRect();
    const QRect spanRect = widget.testSpanVisualRect(0);
    bool sawTopPlacement = false;
    bool sawHiddenLine = false;
    for (int index = 0; index < widget.testSelectedOpcodeTagCount(); ++index) {
        const QVariantMap tag = widget.testSelectedOpcodeTagAt(index);
        const QRect tagRect = tag.value(QStringLiteral("rect")).toRect();
        const QRect lineRect = tag.value(QStringLiteral("lineRect")).toRect();
        const QRect inlineTextRect = tag.value(QStringLiteral("inlineTextRect")).toRect();
        sawTopPlacement = sawTopPlacement || !tag.value(QStringLiteral("bottomPlacement")).toBool();
        if (tag.value(QStringLiteral("lineVisible")).toBool()) {
            expect(lineRect.top() >= spanRect.top() && lineRect.bottom() <= spanRect.bottom(),
                   QStringLiteral("Opcode tag timestamp line should stay inside the selected rectangle."));
            expect(!inlineTextRect.isValid()
                       || !lineRect.adjusted(-2, 0, 2, 0).intersects(inlineTextRect),
                   QStringLiteral("Visible opcode tag timestamp lines should avoid inline text."));
        } else {
            sawHiddenLine = true;
        }
        expect(tagRect.top() >= plot.top() && tagRect.bottom() <= plot.bottom(),
               QStringLiteral("Opcode tags should stay inside the plot vertically."));
    }
    expect(sawTopPlacement,
           QStringLiteral("Opcode tags should prefer the top edge when the selected span is not on the top row."));
    expect(sawHiddenLine,
           QStringLiteral("Opcode timestamp lines that collide with inline text should be suppressed."));

    widget.testSetVerticalScroll(1);
    QApplication::processEvents();
    expectGreater(static_cast<double>(widget.testSelectedOpcodeTagCount()),
                  0.0,
                  QStringLiteral("Opcode tags should remain visible after scrolling the selected span to the top row."));
    for (int index = 0; index < widget.testSelectedOpcodeTagCount(); ++index) {
        expect(widget.testSelectedOpcodeTagAt(index).value(QStringLiteral("bottomPlacement")).toBool(),
               QStringLiteral("Top-row selected spans should place opcode tags on the bottom edge."));
    }

    widget.testSetRowHeight(12);
    QApplication::processEvents();
    const int compactFont = widget.testSelectedOpcodeTagFontPixelSize();
    widget.testSetRowHeight(72);
    QApplication::processEvents();
    expectGreater(static_cast<double>(widget.testSelectedOpcodeTagFontPixelSize()),
                  static_cast<double>(compactFont),
                  QStringLiteral("Opcode tag font should scale with Transaction row height."));
}

void testTransactionWidgetDoubleClickSyncKeepsClickedSpan()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectInterleavedSyntheticSpans();
    QApplication::processEvents();

    int activatedRow = -1;
    QObject::connect(&widget,
                     &CHIron::Gui::TransactionWidget::logicalRowActivated,
                     &widget,
                     [&activatedRow](const int logicalRow) {
                         activatedRow = logicalRow;
                     });

    const QPoint nestedSpanCenter = widget.testSpanCenterPoint(1);
    expect(nestedSpanCenter != QPoint(),
           QStringLiteral("Interleaved double-click fixture should expose the nested span."));
    sendTransactionMouse(widget,
                         QEvent::MouseButtonDblClick,
                         nestedSpanCenter,
                         Qt::LeftButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    expectEqual(activatedRow,
                5,
                QStringLiteral("Double-clicking the nested span should activate its request row."));
    expectEqual(widget.testSelectedSpanIndex(),
                1,
                QStringLiteral("Double-click should select the nested span before table synchronization."));

    widget.setSelectedLogicalRow(activatedRow);
    QApplication::processEvents();
    expectEqual(widget.testSelectedSpanIndex(),
                1,
                QStringLiteral("Main-table sync after double-click should keep the exact clicked transaction span."));
    const QVariantMap nestedSpan = widget.testSpanAt(1);
    expect(widget.testCursorTimestamp() == nestedSpan.value(QStringLiteral("startTimestamp")).toLongLong(),
           QStringLiteral("Cursor should remain on the clicked transaction request time after sync."));
}

void testTransactionWidgetSelectedRowHighlightsRelatedSpanAndMarker()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectSyntheticSpans(12, 36);
    QApplication::processEvents();

    widget.setSelectedLogicalRow(26);
    QApplication::processEvents();

    expectEqual(widget.testSelectedSpanIndex(),
                8,
                QStringLiteral("Selecting a main-table row inside a transaction should select the related span."));
    expectEqual(widget.testSelectedLogicalRow(),
                26,
                QStringLiteral("Main-table row synchronization should preserve the selected row."));
    expect(widget.testHasCursor(),
           QStringLiteral("Main-table row synchronization should place a cursor on the transaction span."));
    const QVariantMap layout = widget.testLayoutState();
    expect(layout.value(QStringLiteral("verticalScrollOffset")).toInt() > 0,
           QStringLiteral("Selecting a transaction row should scroll its lane into view."));
    const QPoint selectedCenter = widget.testSpanCenterPoint(8);
    expect(selectedCenter != QPoint(),
           QStringLiteral("Selected row's transaction span should be visible after synchronization."));

    sendTransactionKey(widget, Qt::Key_M);
    expect(widget.testHasMarker(),
           QStringLiteral("M should place a marker on the selected transaction span."));
    expectEqual(widget.testMarkerLogicalRow(),
                26,
                QStringLiteral("Transaction marker should remember the selected logical row."));

    const QVariantMap state = widget.viewState();
    expect(state.value(QStringLiteral("hasMarker")).toBool(),
           QStringLiteral("Transaction marker should be saved in view state."));
    expectEqual(state.value(QStringLiteral("markerLogicalRow")).toInt(),
                26,
                QStringLiteral("Transaction marker logical row should be saved in view state."));
}

void testTransactionWidgetZoomShortcutsWheelAndHistory()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectSyntheticSpans(4, 120);
    QApplication::processEvents();

    const QRect plot = widget.testPlotRect();
    expect(plot.isValid(), QStringLiteral("Transaction gesture test should expose a plot rect."));

    sendTransactionKey(widget, Qt::Key_Plus, Qt::ControlModifier);
    expectGreater(widget.testHorizontalZoom(),
                  1.0,
                  QStringLiteral("Ctrl+Plus should zoom the Transaction widget in."));
    const double zoomAfterPlus = widget.testHorizontalZoom();

    sendTransactionKey(widget, Qt::Key_Minus, Qt::ControlModifier);
    expect(widget.testHorizontalZoom() < zoomAfterPlus,
           QStringLiteral("Ctrl+Minus should zoom the Transaction widget out."));

    sendTransactionWheel(widget, plot.center(), 120, Qt::ControlModifier);
    expectGreater(widget.testHorizontalZoom(),
                  1.0,
                  QStringLiteral("Ctrl+wheel should zoom the Transaction widget in."));
    const double zoomAfterWheel = widget.testHorizontalZoom();

    sendTransactionKey(widget, Qt::Key_F);
    expectNear(widget.testHorizontalZoom(),
               1.0,
               0.0001,
               QStringLiteral("F should fit the full transaction time range."));

    sendTransactionKey(widget, Qt::Key_B, Qt::ControlModifier);
    expectNear(widget.testHorizontalZoom(),
               zoomAfterWheel,
               0.0001,
               QStringLiteral("Ctrl+B should restore the previous transaction view."));
}

void testTransactionWidgetWheelPanAndKeyboardPan()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectSyntheticSpans(16, 160);
    QApplication::processEvents();

    widget.testSetHorizontalZoom(4.0);
    QApplication::processEvents();
    const auto initialRange = widget.testVisibleTimestampRange();
    const int initialVertical = widget.testLayoutState().value(QStringLiteral("verticalScrollOffset")).toInt();

    sendTransactionWheel(widget, widget.testPlotRect().center(), -120, Qt::NoModifier);
    const int verticalAfterWheel = widget.testLayoutState().value(QStringLiteral("verticalScrollOffset")).toInt();
    expect(verticalAfterWheel > initialVertical,
           QStringLiteral("Plain wheel should vertically scroll transaction stack rows."));

    sendTransactionWheel(widget, widget.testPlotRect().center(), -120, Qt::ShiftModifier);
    const auto rangeAfterShiftWheel = widget.testVisibleTimestampRange();
    expect(rangeAfterShiftWheel.first > initialRange.first,
           QStringLiteral("Shift+wheel should horizontally pan the transaction time range."));

    sendTransactionKey(widget, Qt::Key_Right);
    const auto rangeAfterRight = widget.testVisibleTimestampRange();
    expect(rangeAfterRight.first >= rangeAfterShiftWheel.first,
           QStringLiteral("Right arrow should horizontally pan the transaction view forward."));

    sendTransactionKey(widget, Qt::Key_Up);
    const int verticalAfterUp = widget.testLayoutState().value(QStringLiteral("verticalScrollOffset")).toInt();
    expect(verticalAfterUp < verticalAfterWheel || verticalAfterUp == 0,
           QStringLiteral("Up arrow should vertically pan transaction stack rows upward."));
}

void testTransactionWidgetCtrlShiftWheelAdjustsRowHeight()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectSyntheticSpans(40, 400);
    QApplication::processEvents();

    const QRect plot = widget.testPlotRect();
    const QVariantMap initialLayout = widget.testLayoutState();
    const int initialRowHeight = initialLayout.value(QStringLiteral("rowHeight")).toInt();
    const int initialCapacity = initialLayout.value(QStringLiteral("visibleLaneCapacity")).toInt();
    const double initialZoom = widget.testHorizontalZoom();
    const int initialRulerFontHeight = widget.testRulerFontMetricHeight();
    const int initialInfoBarFontHeight = widget.testInfoBarFontMetricHeight();

    sendTransactionWheel(widget, plot.center(), 120, Qt::ControlModifier | Qt::ShiftModifier);
    const QVariantMap tallerLayout = widget.testLayoutState();
    const int tallerRowHeight = tallerLayout.value(QStringLiteral("rowHeight")).toInt();
    expectGreater(static_cast<double>(tallerRowHeight),
                  static_cast<double>(initialRowHeight),
                  QStringLiteral("Ctrl+Shift+wheel up should increase Transaction row height."));
    expectNear(widget.testHorizontalZoom(),
               initialZoom,
               0.0001,
               QStringLiteral("Ctrl+Shift+wheel should resize rows without triggering horizontal zoom."));
    expect(tallerLayout.value(QStringLiteral("visibleLaneCapacity")).toInt() <= initialCapacity,
           QStringLiteral("Taller Transaction rows should not increase visible lane capacity."));

    QVariantMap savedState = widget.viewState();
    expectEqual(savedState.value(QStringLiteral("rowHeight")).toInt(),
                tallerRowHeight,
                QStringLiteral("Transaction row height should be persisted in view state."));

    CHIron::Gui::TransactionWidget restored;
    showTransactionForTest(restored);
    restored.testInjectSyntheticSpans(40, 400);
    restored.restoreViewState(savedState);
    QApplication::processEvents();
    expectEqual(restored.testLayoutState().value(QStringLiteral("rowHeight")).toInt(),
                tallerRowHeight,
                QStringLiteral("Transaction row height should restore with the view state."));

    sendTransactionWheel(widget, plot.center(), -120, Qt::ControlModifier | Qt::ShiftModifier);
    expectEqual(widget.testLayoutState().value(QStringLiteral("rowHeight")).toInt(),
                initialRowHeight,
                QStringLiteral("Ctrl+Shift+wheel down should decrease Transaction row height by one step."));

    for (int i = 0; i < 48; ++i) {
        sendTransactionWheel(widget, plot.center(), -120, Qt::ControlModifier | Qt::ShiftModifier);
    }
    const QVariantMap compactLayout = widget.testLayoutState();
    const int compactRowHeight = compactLayout.value(QStringLiteral("rowHeight")).toInt();
    expectEqual(compactRowHeight,
                12,
                QStringLiteral("Transaction row height should clamp to the compact minimum."));
    expect(compactLayout.value(QStringLiteral("visibleLaneCapacity")).toInt() > initialCapacity,
           QStringLiteral("Smaller Transaction rows should expose more visible lanes."));
    const int compactFontSize = widget.testSpanLabelFontPixelSize();
    expect(compactFontSize < 11,
           QStringLiteral("Transaction rectangle label font should shrink for compact row heights."));
    expectEqual(widget.testRulerFontMetricHeight(),
                initialRulerFontHeight,
                QStringLiteral("Transaction ruler font should not shrink with compact row heights."));
    expectEqual(widget.testInfoBarFontMetricHeight(),
                initialInfoBarFontHeight,
                QStringLiteral("Transaction info-bar font should not shrink with compact row heights."));

    for (int i = 0; i < 48; ++i) {
        sendTransactionWheel(widget, plot.center(), 120, Qt::ControlModifier | Qt::ShiftModifier);
    }
    expectGreater(static_cast<double>(widget.testSpanLabelFontPixelSize()),
                  static_cast<double>(compactFontSize),
                  QStringLiteral("Transaction rectangle label font should grow when row height grows."));
}

void testTransactionWidgetLeftDragGesturesAndOverlay()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectSyntheticSpans(8, 160);
    QApplication::processEvents();

    const QRect plot = widget.testPlotRect();
    sendTransactionKey(widget, Qt::Key_Plus, Qt::ControlModifier);
    const double zoomBeforeRange = widget.testHorizontalZoom();

    const QPoint rangeStart(plot.left() + plot.width() / 4, plot.center().y());
    const QPoint rangeEnd(plot.left() + plot.width() / 2, plot.center().y() + 1);
    sendTransactionMouse(widget,
                         QEvent::MouseButtonPress,
                         rangeStart,
                         Qt::LeftButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    sendTransactionMouse(widget,
                         QEvent::MouseMove,
                         rangeEnd,
                         Qt::NoButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    expectEqual(widget.testDragModeName(),
                QStringLiteral("RangeZoom"),
                QStringLiteral("Horizontal left-drag should enter range zoom mode."));
    sendTransactionMouse(widget,
                         QEvent::MouseButtonRelease,
                         rangeEnd,
                         Qt::LeftButton,
                         Qt::NoButton,
                         Qt::NoModifier);
    expectGreater(widget.testHorizontalZoom(),
                  zoomBeforeRange,
                  QStringLiteral("Horizontal left-drag should zoom to the selected range."));

    const QPoint diagonalStart(plot.left() + 40, plot.top() + 110);
    const QPoint diagonalEnd(diagonalStart.x() + 80, diagonalStart.y() - 72);
    const double zoomBeforeDiagonal = widget.testHorizontalZoom();
    sendTransactionMouse(widget,
                         QEvent::MouseButtonPress,
                         diagonalStart,
                         Qt::LeftButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    sendTransactionMouse(widget,
                         QEvent::MouseMove,
                         diagonalEnd,
                         Qt::NoButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    expectEqual(widget.testDragModeName(),
                QStringLiteral("ZoomIn2x"),
                QStringLiteral("Lower-left to upper-right drag should enter zoom-in gesture mode."));
    expect(widget.testGestureOverlayVisible(),
           QStringLiteral("Diagonal transaction gesture should expose an overlay while dragging."));
    sendTransactionMouse(widget,
                         QEvent::MouseButtonRelease,
                         diagonalEnd,
                         Qt::LeftButton,
                         Qt::NoButton,
                         Qt::NoModifier);
    expectNear(widget.testHorizontalZoom(),
               zoomBeforeDiagonal * 2.0,
               0.05,
               QStringLiteral("Diagonal zoom-in gesture should double the horizontal zoom."));

    const QPoint verticalStart(plot.left() + 80, plot.top() + 30);
    const QPoint verticalEnd(verticalStart.x() + 2, verticalStart.y() + 80);
    sendTransactionMouse(widget,
                         QEvent::MouseButtonPress,
                         verticalStart,
                         Qt::LeftButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    sendTransactionMouse(widget,
                         QEvent::MouseMove,
                         verticalEnd,
                         Qt::NoButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    expectEqual(widget.testDragModeName(),
                QStringLiteral("ZoomFull"),
                QStringLiteral("Vertical left-drag should enter zoom-full gesture mode."));
    sendTransactionMouse(widget,
                         QEvent::MouseButtonRelease,
                         verticalEnd,
                         Qt::LeftButton,
                         Qt::NoButton,
                         Qt::NoModifier);
    expectNear(widget.testHorizontalZoom(),
               1.0,
               0.0001,
               QStringLiteral("Vertical left-drag should fit the full transaction time range."));
}

void testTransactionWidgetRangeZoomOverlayStaysTransparent()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectOverlappingSyntheticSpans();
    QApplication::processEvents();

    const QRect plot = widget.testPlotRect();
    const QPoint rangeStart(plot.left() + plot.width() / 5, plot.center().y());
    const QPoint rangeEnd(plot.left() + plot.width() * 3 / 5, plot.center().y() + 1);
    sendTransactionMouse(widget,
                         QEvent::MouseButtonPress,
                         rangeStart,
                         Qt::LeftButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    sendTransactionMouse(widget,
                         QEvent::MouseMove,
                         rangeEnd,
                         Qt::NoButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    expectEqual(widget.testDragModeName(),
                QStringLiteral("RangeZoom"),
                QStringLiteral("Range overlay fixture should enter horizontal range zoom mode."));

    const QImage image = renderTransactionImage(widget);
    const QRect selection(std::min(rangeStart.x(), rangeEnd.x()),
                          plot.top(),
                          std::abs(rangeEnd.x() - rangeStart.x()),
                          plot.height());
    const QRect sample = selection.adjusted(6, 6, -6, -6).intersected(plot);
    int opaqueTransactionFillPixels = 0;
    int translucentBluePixels = 0;
    for (int y = sample.top(); y <= sample.bottom(); y += 5) {
        for (int x = sample.left(); x <= sample.right(); x += 5) {
            const QColor pixel = QColor::fromRgba(image.pixel(x, y));
            if (pixel.red() >= 225 && pixel.red() <= 235
                && pixel.green() >= 238 && pixel.green() <= 248
                && pixel.blue() >= 248) {
                ++opaqueTransactionFillPixels;
            }
            if (pixel.blue() > pixel.red() + 10
                && pixel.blue() > pixel.green()
                && pixel.red() >= 205
                && pixel.green() >= 220) {
                ++translucentBluePixels;
            }
        }
    }
    expect(translucentBluePixels > 0,
           QStringLiteral("Range zoom overlay should paint a visible translucent blue tint."));
    expect(opaqueTransactionFillPixels < translucentBluePixels,
           QStringLiteral("Range zoom overlay should not be repainted by an inherited opaque transaction brush."));

    sendTransactionMouse(widget,
                         QEvent::MouseButtonRelease,
                         rangeEnd,
                         Qt::LeftButton,
                         Qt::NoButton,
                         Qt::NoModifier);
}

void testTransactionWidgetLargeTimestampRangeZoomAndScrollStayReachable()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectSyntheticSpans(8, 160);
    widget.testScaleSyntheticTimestamps(30000000);
    QApplication::processEvents();

    widget.testSetHorizontalZoom(4.0);
    QApplication::processEvents();
    const auto zoomedRange = widget.testVisibleTimestampRange();
    expect(zoomedRange.second > 0,
           QStringLiteral("Large timestamp regression fixture should expose a valid zoomed range."));

    sendTransactionKey(widget, Qt::Key_End);
    const auto endRange = widget.testVisibleTimestampRange();
    expect(endRange.first > static_cast<qint64>(std::numeric_limits<int>::max()),
           QStringLiteral("Transaction End key should reach timestamp regions beyond a 32-bit scrollbar value."));

    const QVariantMap beforeZoomLayout = widget.testLayoutState();
    const std::uint64_t beforeZoomOffset =
        beforeZoomLayout.value(QStringLiteral("horizontalScrollOffset")).toULongLong();
    expect(beforeZoomOffset > static_cast<std::uint64_t>(std::numeric_limits<int>::max()),
           QStringLiteral("Transaction internal scroll offset should remain a large timestamp offset."));

    widget.testSetCursorFromPosition(widget.testPlotRect().center());
    const qint64 cursorBefore = widget.testCursorTimestamp();
    expect(widget.testHasCursor(),
           QStringLiteral("Large timestamp regression should start with a visible transaction cursor."));

    const QRect plot = widget.testPlotRect();
    const QPoint rangeStart(plot.left() + plot.width() / 4, plot.center().y());
    const QPoint rangeEnd(plot.left() + plot.width() * 3 / 4, plot.center().y() + 1);
    sendTransactionMouse(widget,
                         QEvent::MouseButtonPress,
                         rangeStart,
                         Qt::LeftButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    sendTransactionMouse(widget,
                         QEvent::MouseMove,
                         rangeEnd,
                         Qt::NoButton,
                         Qt::LeftButton,
                         Qt::NoModifier);
    sendTransactionMouse(widget,
                         QEvent::MouseButtonRelease,
                         rangeEnd,
                         Qt::LeftButton,
                         Qt::NoButton,
                         Qt::NoModifier);

    const auto rangeZoomed = widget.testVisibleTimestampRange();
    expect(rangeZoomed.first > static_cast<qint64>(std::numeric_limits<int>::max()),
           QStringLiteral("Transaction range zoom should keep late timestamp regions instead of snapping to the early trace."));
    expect(rangeZoomed.first <= cursorBefore && cursorBefore <= rangeZoomed.second,
           QStringLiteral("Transaction range zoom around the cursor should leave that cursor timestamp inside the view."));
    expectEqual(static_cast<int>(widget.testCursorTimestamp() == cursorBefore),
                1,
                QStringLiteral("Transaction range zoom should not retarget the existing cursor timestamp."));

    sendTransactionKey(widget, Qt::Key_Home);
    const auto homeRange = widget.testVisibleTimestampRange();
    expect(homeRange.first < rangeZoomed.first,
           QStringLiteral("Transaction Home key should still reach earlier timestamp regions after a large-range zoom."));
    sendTransactionKey(widget, Qt::Key_End);
    const auto finalRange = widget.testVisibleTimestampRange();
    expect(finalRange.first >= rangeZoomed.first,
           QStringLiteral("Transaction late timestamp region should remain reachable after Home/End scrolling."));
}

void testTransactionWidgetLeftDragPanAcrossDenseRows()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectSyntheticSpans(12, 20000);
    QApplication::processEvents();

    widget.testSetHorizontalZoom(4.0);
    widget.testSetHorizontalScroll(5000);
    widget.testSetVerticalScroll(4);
    QApplication::processEvents();

    const auto initialRange = widget.testVisibleTimestampRange();
    const int initialVertical = widget.testLayoutState().value(QStringLiteral("verticalScrollOffset")).toInt();
    const QRect plot = widget.testPlotRect();
    const QPoint panStart(plot.left() + plot.width() / 2, plot.top() + 40);
    const QPoint panEnd(panStart.x() - 120, panStart.y() + 70);

    sendTransactionMouse(widget,
                         QEvent::MouseButtonPress,
                         panStart,
                         Qt::LeftButton,
                         Qt::LeftButton,
                         Qt::ShiftModifier);
    sendTransactionMouse(widget,
                         QEvent::MouseMove,
                         panEnd,
                         Qt::NoButton,
                         Qt::LeftButton,
                         Qt::ShiftModifier);
    expectEqual(widget.testDragModeName(),
                QStringLiteral("Pan"),
                QStringLiteral("Shift left-drag should enter transaction pan mode."));
    sendTransactionMouse(widget,
                         QEvent::MouseButtonRelease,
                         panEnd,
                         Qt::LeftButton,
                         Qt::NoButton,
                         Qt::ShiftModifier);

    const auto pannedRange = widget.testVisibleTimestampRange();
    const int pannedVertical = widget.testLayoutState().value(QStringLiteral("verticalScrollOffset")).toInt();
    expect(pannedRange.first > initialRange.first,
           QStringLiteral("Transaction pan gesture should move the time range forward when dragged left."));
    expect(pannedVertical < initialVertical,
           QStringLiteral("Transaction pan gesture should move vertically across rows."));
    expect(widget.testUsesDensePaint(),
           QStringLiteral("Dense transaction rows should remain in dense paint mode after panning."));
}

void testTransactionWidgetPaintCandidatesStayViewportBounded()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectSyntheticSpans(80, 20000);
    widget.testSetHorizontalZoom(20.0);
    widget.testSetHorizontalScroll(60000);
    widget.testSetVerticalScroll(40);
    QApplication::processEvents();

    const int visibleCandidates = widget.testVisiblePaintCandidateCount();
    expect(visibleCandidates > 0,
           QStringLiteral("Viewport-indexed transaction paint should still find visible spans."));
    expect(visibleCandidates < widget.testSpanCount() / 10,
           QStringLiteral("Transaction paint should use a viewport-bounded candidate set instead of scanning every span."));

    const QVariantMap layout = widget.testLayoutState();
    const int visibleRows = layout.value(QStringLiteral("visibleLaneCapacity")).toInt();
    expect(visibleCandidates <= std::max(1, visibleRows + 1) * 200,
           QStringLiteral("Visible transaction paint candidates should stay bounded by visible rows and time range."));

    widget.testSetRowHeight(20);
    QApplication::processEvents();
    const int compactVisibleCandidates = widget.testVisiblePaintCandidateCount();
    const QVariantMap compactLayout = widget.testLayoutState();
    const int compactVisibleRows = compactLayout.value(QStringLiteral("visibleLaneCapacity")).toInt();
    expect(compactVisibleCandidates <= std::max(1, compactVisibleRows + 1) * 200,
           QStringLiteral("Compact transaction rows should still keep paint candidates bounded by visible rows."));
}

void testTransactionWidgetDenseRenderStaysInteractiveForLargeView()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectSyntheticSpans(80, 120000);
    widget.testSetHorizontalZoom(1.0);
    widget.testSetVerticalScroll(20);
    QApplication::processEvents();

    expect(widget.testUsesDensePaint(),
           QStringLiteral("Large transaction render benchmark fixture should use dense paint."));

    QImage image(widget.size(), QImage::Format_ARGB32_Premultiplied);
    QElapsedTimer timer;
    timer.start();
    for (int iteration = 0; iteration < 3; ++iteration) {
        image.fill(Qt::transparent);
        QPainter painter(&image);
        widget.render(&painter);
        painter.end();
    }
    const qint64 averageMs = timer.elapsed() / 3;
    expect(averageMs < 250,
           QStringLiteral("Dense transaction render should stay responsive for large synthetic views."));

    const QPoint firstSpanCenter = widget.testSpanCenterPoint(1620);
    expect(firstSpanCenter != QPoint(),
           QStringLiteral("Dense render responsiveness fixture should expose a visible span."));
    expect(transactionImageHasNonWhiteNear(widget, firstSpanCenter, 5),
           QStringLiteral("Dense render responsiveness fixture should still paint visible span content."));
}

void testTransactionWidgetLargeTraceZoomKeepsNonSelectedLabels()
{
    CHIron::Gui::TransactionWidget widget;
    showTransactionForTest(widget);
    widget.testInjectSyntheticSpans(80, 20000);
    widget.testSetHorizontalZoom(30000.0);
    widget.testSetVerticalScroll(0);
    QApplication::processEvents();

    expect(!widget.testUsesDensePaint(),
           QStringLiteral("Large transaction traces should leave dense mode when the visible viewport is sparse enough for labels."));

    const QRect firstRect = widget.testSpanVisualRect(0);
    expect(firstRect.width() >= 160,
           QStringLiteral("Zoomed large-trace fixture should make the first non-selected rectangle wide enough for an inline label."));
    const QString label = widget.testInlineLabelForSpan(0, firstRect.width() - 6);
    expect(label.contains(QStringLiteral("ReadNoSnp"))
               && label.contains(QStringLiteral("0x")),
           QStringLiteral("Non-selected transaction rectangles should keep opcode/address labels when zoomed in."));
}

void testTraceSessionConcurrentTimelineAddressReadsKeepCacheStable()
{
    using ReqFlit = CHIron::Gui::ReqFlit;
    using DatFlit = CHIron::Gui::DatFlit;
    using SnpFlit = CHIron::Gui::SnpFlit;

    const CLog::Parameters parameters = makeDefaultTestParameters();

    const auto serializeReq = [](const ReqFlit& flit, TestFlitBitWriter& writer) {
        appendEbReqFlitForTest(flit, writer);
    };
    const auto serializeDat = [](const DatFlit& flit, TestFlitBitWriter& writer) {
        appendEbDatFlitForTest(flit, writer);
    };
    const auto serializeSnp = [](const SnpFlit& flit, TestFlitBitWriter& writer) {
        appendEbSnpFlitForTest(flit, writer);
    };

    std::vector<std::vector<CLog::CLogB::TagCHIRecords::Record>> blocks;
    blocks.reserve(24);
    for (int blockIndex = 0; blockIndex < 24; ++blockIndex) {
        std::vector<CLog::CLogB::TagCHIRecords::Record> block;
        block.reserve(48);
        for (int row = 0; row < 48; ++row) {
            const std::uint16_t nodeId = static_cast<std::uint16_t>(1 + (blockIndex + row) % 8);
            const std::uint32_t timeShift = 1 + static_cast<std::uint32_t>(row % 5);
            if (row % 3 == 0) {
                ReqFlit flit{};
                flit.SrcID() = nodeId;
                flit.TgtID() = static_cast<std::uint16_t>(16 + (row % 8));
                flit.TxnID() = static_cast<std::uint16_t>((blockIndex * 7 + row) & 0xFF);
                flit.Opcode() = CHI::Eb::Opcodes::REQ::ReadNoSnp;
                flit.Size() = CHI::Eb::Sizes::B64;
                flit.AllowRetry() = 1;
                flit.Addr() = 0x100000ULL + static_cast<std::uint64_t>(blockIndex * 4096 + row * 64);
                block.push_back(makeSerializedRecord(CLog::Channel::TXREQ,
                                                     nodeId,
                                                     timeShift,
                                                     flit,
                                                     serializeReq));
            } else if (row % 3 == 1) {
                DatFlit flit{};
                flit.SrcID() = static_cast<std::uint16_t>(16 + (row % 8));
                flit.TgtID() = nodeId;
                flit.HomeNID() = flit.SrcID();
                flit.TxnID() = static_cast<std::uint16_t>((blockIndex * 7 + row) & 0xFF);
                flit.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
                flit.RespErr() = CHI::Eb::RespErrs::OK;
                flit.Resp() = CHI::Eb::Resps::CompData_UC;
                flit.DBID() = static_cast<std::uint16_t>(row & 0xFF);
                flit.DataID() = static_cast<std::uint8_t>((row % 2) == 0 ? 0 : 2);
                flit.BE() = 0xFFFFFFFFU;
                for (std::size_t word = 0; word < DatFlit::DATA_WIDTH / 64U; ++word) {
                    flit.Data()[word] = 0x0102030405060708ULL
                        + static_cast<std::uint64_t>(blockIndex * 101 + row * 17 + word);
                }
                block.push_back(makeSerializedRecord(CLog::Channel::RXDAT,
                                                     nodeId,
                                                     timeShift,
                                                     flit,
                                                     serializeDat));
            } else {
                SnpFlit flit{};
                flit.SrcID() = static_cast<std::uint16_t>(16 + (row % 8));
                flit.TxnID() = static_cast<std::uint16_t>((blockIndex * 5 + row) & 0xFF);
                flit.FwdNID() = nodeId;
                flit.FwdTxnID() = static_cast<std::uint16_t>((row + 3) & 0xFF);
                flit.Opcode() = CHI::Eb::Opcodes::SNP::SnpMakeInvalid;
                flit.Addr() = (0x200000ULL + static_cast<std::uint64_t>(blockIndex * 4096 + row * 64)) >> 3U;
                block.push_back(makeSerializedRecord(CLog::Channel::TXSNP,
                                                     nodeId,
                                                     timeShift,
                                                     flit,
                                                     serializeSnp));
            }
        }
        blocks.push_back(std::move(block));
    }

    QTemporaryDir tempDir;
    expect(tempDir.isValid(), QStringLiteral("Temporary directory creation failed."));
    const QString tracePath = tempDir.filePath(QStringLiteral("concurrent_timeline_address_cache.clogb"));
    writeLengthOnlyTraceBlocks(tracePath, parameters, std::move(blocks));

    CHIron::Gui::TraceSessionOptions options;
    options.maxCachedBlocks = 1;
    options.maxCachedPages = 1;
    options.pageSize = 16;
    CHIron::Gui::CLogBTraceLoadError error;
    std::shared_ptr<CHIron::Gui::TraceSession> session;
    expect(CHIron::Gui::TraceSession::open(tracePath, session, error, options),
           error.summary.isEmpty()
               ? QStringLiteral("Trace session should open the concurrent cache stress trace.")
               : error.summary);
    expect(session != nullptr, QStringLiteral("Concurrent cache stress session should be created."));

    std::atomic_bool failed = false;
    std::mutex errorMutex;
    QString failureText;
    const auto recordFailure = [&](const QString& text) {
        bool expected = false;
        if (failed.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
            const std::lock_guard lock(errorMutex);
            failureText = text;
        }
    };

    std::jthread fastReader([&](std::stop_token stopToken) {
        for (int iteration = 0; iteration < 40 && !failed.load(std::memory_order_relaxed); ++iteration) {
            for (std::size_t blockIndex = 0;
                 blockIndex < session->metadata().blocks.size() && !failed.load(std::memory_order_relaxed);
                 ++blockIndex) {
                std::vector<CHIron::Gui::CLogBTraceFastRecordInfo> records;
                CHIron::Gui::CLogBTraceLoadError localError;
                const std::size_t rowCount =
                    static_cast<std::size_t>(session->metadata().blocks[blockIndex].recordCount);
                if (!session->readFastRecords(blockIndex, 0, rowCount, records, localError, stopToken)) {
                    recordFailure(localError.summary.isEmpty()
                                      ? QStringLiteral("Fast-record reader failed.")
                                      : localError.summary);
                    return;
                }
                if (records.size() != rowCount) {
                    recordFailure(QStringLiteral("Fast-record reader returned an unexpected row count."));
                    return;
                }
            }
        }
    });

    std::jthread rowReader([&](std::stop_token stopToken) {
        const std::uint64_t totalRows = session->totalRows();
        for (int iteration = 0; iteration < 40 && !failed.load(std::memory_order_relaxed); ++iteration) {
            for (std::uint64_t rowStart = 0;
                 rowStart < totalRows && !failed.load(std::memory_order_relaxed);
                 rowStart += 13) {
                std::vector<CHIron::Gui::FlitRecord> rows;
                CHIron::Gui::CLogBTraceLoadError localError;
                const std::uint64_t rowCount = std::min<std::uint64_t>(17, totalRows - rowStart);
                if (!session->readRows(rowStart, rowCount, rows, localError, {}, stopToken)) {
                    recordFailure(localError.summary.isEmpty()
                                      ? QStringLiteral("Decoded-row reader failed.")
                                      : localError.summary);
                    return;
                }
                if (rows.size() != rowCount) {
                    recordFailure(QStringLiteral("Decoded-row reader returned an unexpected row count."));
                    return;
                }
            }
        }
    });

    fastReader.join();
    rowReader.join();

    expect(!failed.load(std::memory_order_relaxed),
           failureText.isEmpty()
               ? QStringLiteral("Concurrent session readers should not lose cached blocks.")
               : failureText);
    expect(session->cachedBlockCount() <= session->maxCachedBlocks(),
           QStringLiteral("Concurrent session readers should keep the block cache within capacity."));
    expect(session->cachedPageCount() <= session->maxCachedPages(),
           QStringLiteral("Concurrent session readers should keep the page cache within capacity."));
}

}  // namespace

int main(int argc, char* argv[])
{
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
#if defined(Q_OS_WIN)
        qputenv("QT_QPA_PLATFORM", QByteArray("windows"));
#else
        qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
#endif
    }
    QApplication app(argc, argv);
    (void) app;

    const std::vector<std::pair<QString, TestFunction>> tests = {
        {QStringLiteral("AddressHexPrefixIsIgnored"), testAddressHexPrefixIsIgnored},
        {QStringLiteral("NumericAddressAndDbidFiltersMatchExpectedRows"), testNumericAddressAndDbidFiltersMatchExpectedRows},
        {QStringLiteral("SearchHighlightModeKeepsRowsAndMarksMatches"), testSearchHighlightModeKeepsRowsAndMarksMatches},
        {QStringLiteral("TraceTableTimestampDisplayUsesThousandsSeparators"), testTraceTableTimestampDisplayUsesThousandsSeparators},
        {QStringLiteral("FlitTableDeleteUndoRedoForRowBackedRows"), testFlitTableDeleteUndoRedoForRowBackedRows},
        {QStringLiteral("FlitTableTimestampInsertDeleteUndoRedoKeepsOrder"), testFlitTableTimestampInsertDeleteUndoRedoKeepsOrder},
        {QStringLiteral("FlitTableSessionBackedDeleteUndoStreamsRows"), testFlitTableSessionBackedDeleteUndoStreamsRows},
        {QStringLiteral("MainWindowTraceToolbarFoldsAndUnfolds"), testMainWindowTraceToolbarFoldsAndUnfolds},
        {QStringLiteral("MainWindowKeepsIndependentTraceSessions"), testMainWindowKeepsIndependentTraceSessions},
        {QStringLiteral("ClipboardWidgetOrderingDuplicatesFilteringAndDeletion"), testClipboardWidgetOrderingDuplicatesFilteringAndDeletion},
        {QStringLiteral("ClipboardSingleClickHighlightsRelatedTransactionRows"), testClipboardSingleClickHighlightsRelatedTransactionRows},
        {QStringLiteral("ClipboardInsertSelectedIndexedTransaction"), testClipboardInsertSelectedIndexedTransaction},
        {QStringLiteral("ClipboardInsertTransactionsWithSameAddressModes"), testClipboardInsertTransactionsWithSameAddressModes},
        {QStringLiteral("ClipboardInsertTransactionsWithSameAddressSparseNonmatches"), testClipboardInsertTransactionsWithSameAddressSparseNonmatches},
        {QStringLiteral("ClipboardBatchInsertionSkipsExistingRowsWithoutReordering"), testClipboardBatchInsertionSkipsExistingRowsWithoutReordering},
        {QStringLiteral("ClipboardBatchInsertionPreservesLogicalOrder"), testClipboardBatchInsertionPreservesLogicalOrder},
        {QStringLiteral("ClipboardBatchInsertionMergesExistingSubsetByLogicalRow"), testClipboardBatchInsertionMergesExistingSubsetByLogicalRow},
        {QStringLiteral("ClipboardAddressBatchCancelControlVisible"), testClipboardAddressBatchCancelControlVisible},
        {QStringLiteral("ClipboardSameAddressInsertionKeepsEventLoopResponsive"), testClipboardSameAddressInsertionKeepsEventLoopResponsive},
        {QStringLiteral("ClipboardInsertionSortsOnlyEditedRows"), testClipboardInsertionSortsOnlyEditedRows},
        {QStringLiteral("ClipboardVisiblePageMaterializesVisibleColumns"), testClipboardVisiblePageMaterializesVisibleColumns},
        {QStringLiteral("ClipboardReadOnlyEditableModifiedAndNavigation"), testClipboardReadOnlyEditableModifiedAndNavigation},
        {QStringLiteral("ClipboardCsvSaveIncludesFilteredRows"), testClipboardCsvSaveIncludesFilteredRows},
        {QStringLiteral("ClipboardCLogBSaveAvailabilityAndRoundTrip"), testClipboardCLogBSaveAvailabilityAndRoundTrip},
        {QStringLiteral("ClipboardScopesAreSessionLocalAndGlobal"), testClipboardScopesAreSessionLocalAndGlobal},
        {QStringLiteral("MainWindowMarkersArePerSessionAndPersist"), testMainWindowMarkersArePerSessionAndPersist},
        {QStringLiteral("MainWindowMarkerUndoRedoAndUnifiedOrdering"), testMainWindowMarkerUndoRedoAndUnifiedOrdering},
        {QStringLiteral("TraceIssueSeverityAndErrorsWidget"), testTraceIssueSeverityAndErrorsWidget},
        {QStringLiteral("ErrorsDockSourceDispositionAndSeverityFilters"), testErrorsDockSourceDispositionAndSeverityFilters},
        {QStringLiteral("MarkerWidgetSearchFiltersNameAndMemo"), testMarkerWidgetSearchFiltersNameAndMemo},
        {QStringLiteral("MarkerJsonPersistsStickyState"), testMarkerJsonPersistsStickyState},
        {QStringLiteral("MainWindowPreservesPerSessionTableDetails"), testMainWindowPreservesPerSessionTableDetails},
        {QStringLiteral("MainWindowPreservesPerSessionWidgetState"), testMainWindowPreservesPerSessionWidgetState},
        {QStringLiteral("MainWindowSessionSwitchKeepsTimelineAndAddressStable"), testMainWindowSessionSwitchKeepsTimelineAndAddressStable},
        {QStringLiteral("MainWindowTransactionWidgetInstancesAndStateArePerSession"), testMainWindowTransactionWidgetInstancesAndStateArePerSession},
        {QStringLiteral("MainWindowClosingSessionDoesNotClearActiveTransactionWidget"), testMainWindowClosingSessionDoesNotClearActiveTransactionWidget},
        {QStringLiteral("MainWindowCacheWidgetInstancesAndStateArePerSession"), testMainWindowCacheWidgetInstancesAndStateArePerSession},
        {QStringLiteral("MainWindowClosingSessionDoesNotClearActiveCacheWidget"), testMainWindowClosingSessionDoesNotClearActiveCacheWidget},
        {QStringLiteral("MainWindowCloseSessionWithActiveTransactionBuildIgnoresLateCompletion"), testMainWindowCloseSessionWithActiveTransactionBuildIgnoresLateCompletion},
        {QStringLiteral("MainWindowDiagnosticsIncludeTransactionWidgetState"), testMainWindowDiagnosticsIncludeTransactionWidgetState},
        {QStringLiteral("MainWindowDiagnosticsIncludeCacheWidgetState"), testMainWindowDiagnosticsIncludeCacheWidgetState},
        {QStringLiteral("CoreRetryReissueAcceptedOnRNFAndSNF"), testCoreRetryReissueAcceptedOnRNFAndSNF},
        {QStringLiteral("XactionIndexRetryBoundaryReplaysPCrdGrant"), testXactionIndexRetryBoundaryReplaysPCrdGrant},
        {QStringLiteral("MainWindowTransactionRefreshFollowsOwningXactionSession"), testMainWindowTransactionRefreshFollowsOwningXactionSession},
        {QStringLiteral("MainWindowTransactionSessionSwitchesDoNotRebuild"), testMainWindowTransactionSessionSwitchesDoNotRebuild},
        {QStringLiteral("MainWindowPreservesPerSessionTopologySelection"), testMainWindowPreservesPerSessionTopologySelection},
        {QStringLiteral("MainWindowSessionCloseActions"), testMainWindowSessionCloseActions},
        {QStringLiteral("MainWindowReloadsOnlyActiveSession"), testMainWindowReloadsOnlyActiveSession},
        {QStringLiteral("MainWindowReloadFailureKeepsOldSessionUsable"), testMainWindowReloadFailureKeepsOldSessionUsable},
        {QStringLiteral("MainWindowDiagnosticsSnapshotSummarizesMultipleSessions"), testMainWindowDiagnosticsSnapshotSummarizesMultipleSessions},
        {QStringLiteral("MainWindowSessionBackedModelsStayLazyAndCachesBounded"), testMainWindowSessionBackedModelsStayLazyAndCachesBounded},
        {QStringLiteral("MainWindowLoadFailureDoesNotCorruptExistingSessions"), testMainWindowLoadFailureDoesNotCorruptExistingSessions},
        {QStringLiteral("MainWindowStatisticsCompletionStaysWithOriginSession"), testMainWindowStatisticsCompletionStaysWithOriginSession},
        {QStringLiteral("MainWindowCloseSessionWithActiveStatisticsIgnoresLateChunks"), testMainWindowCloseSessionWithActiveStatisticsIgnoresLateChunks},
        {QStringLiteral("DeniedFlitFilterShowsOnlyDeniedXactionRows"), testDeniedFlitFilterShowsOnlyDeniedXactionRows},
        {QStringLiteral("DeniedFlitFilterMatchesRedXactionBulletsForTraceSession"), testDeniedFlitFilterMatchesRedXactionBulletsForTraceSession},
        {QStringLiteral("ErrorsDockShowsDeniedXactionRows"), testErrorsDockShowsDeniedXactionRows},
        {QStringLiteral("DoubleClickTransactionHighlightMarksRelatedRows"), testDoubleClickTransactionHighlightMarksRelatedRows},
        {QStringLiteral("ReadNoSnpTransactionHighlightIncludesAllCompDataBeats"), testReadNoSnpTransactionHighlightIncludesAllCompDataBeats},
        {QStringLiteral("XactionIndexPersistsRowsAfterChunkWriteFlush"), testXactionIndexPersistsRowsAfterChunkWriteFlush},
        {QStringLiteral("CacheLineLifetimeReplayBuildsReadCleanOpenEndedLifetimes"), testCacheLineLifetimeReplayBuildsReadCleanOpenEndedLifetimes},
        {QStringLiteral("CacheLineStateReplayBuildsRequestedRnAddressSpans"), testCacheLineStateReplayBuildsRequestedRnAddressSpans},
        {QStringLiteral("CacheLineStateReplayAppliesReqTxdatCopyBack"), testCacheLineStateReplayAppliesReqTxdatCopyBack},
        {QStringLiteral("CacheLineStateReplayToleratesNonStateDatFieldDenials"), testCacheLineStateReplayToleratesNonStateDatFieldDenials},
        {QStringLiteral("XactionIndexPersistsCacheStateReplayIssues"), testXactionIndexPersistsCacheStateReplayIssues},
        {QStringLiteral("XactionIndexAcceptsHomeToRequesterSnoop"), testXactionIndexAcceptsHomeToRequesterSnoop},
        {QStringLiteral("TraceCacheMinimapMainTraceBuildsLaneOverlayAndSegments"), testTraceCacheMinimapMainTraceBuildsLaneOverlayAndSegments},
        {QStringLiteral("TraceCacheMinimapTagsStackAndAnchorToLaneRightEdge"), testTraceCacheMinimapTagsStackAndAnchorToLaneRightEdge},
        {QStringLiteral("TraceCacheMinimapAddingLaneDuringBuildFinishesBoth"), testTraceCacheMinimapAddingLaneDuringBuildFinishesBoth},
        {QStringLiteral("TraceCacheMinimapDeletingLaneKeepsRemainingBuildResult"), testTraceCacheMinimapDeletingLaneKeepsRemainingBuildResult},
        {QStringLiteral("TraceCacheMinimapStateJumpActions"), testTraceCacheMinimapStateJumpActions},
        {QStringLiteral("TraceCacheMinimapChangeJumpActions"), testTraceCacheMinimapChangeJumpActions},
        {QStringLiteral("TraceCacheMinimapClipboardBuildsFromSourceRows"), testTraceCacheMinimapClipboardBuildsFromSourceRows},
        {QStringLiteral("TraceCacheMinimapClipboardStateJumpActions"), testTraceCacheMinimapClipboardStateJumpActions},
        {QStringLiteral("TraceCacheMinimapClipboardChangeJumpActions"), testTraceCacheMinimapClipboardChangeJumpActions},
        {QStringLiteral("TraceCacheMinimapClipboardMergesRowsInsideSameSourceState"), testTraceCacheMinimapClipboardMergesRowsInsideSameSourceState},
        {QStringLiteral("TraceCacheMinimapClipboardSlicesStateReturnTransitions"), testTraceCacheMinimapClipboardSlicesStateReturnTransitions},
        {QStringLiteral("BeforeSamRequestChannelsDisplayAsReqWithMarker"), testBeforeSamRequestChannelsDisplayAsReqWithMarker},
        {QStringLiteral("SnpTargetColumnDisplaysCaptureNodeAndStaysDimmed"), testSnpTargetColumnDisplaysCaptureNodeAndStaysDimmed},
        {QStringLiteral("SuggestedFilterWorkerCountIsBounded"), testSuggestedFilterWorkerCountIsBounded},
        {QStringLiteral("FastIndexDescriptorValidationRejectsCorruption"), testFastIndexDescriptorValidationRejectsCorruption},
        {QStringLiteral("FastRecordRangeFilterPreservesMatchOrder"), testFastRecordRangeFilterPreservesMatchOrder},
        {QStringLiteral("OptionalFieldIndexProgressUsesRecordCount"), testOptionalFieldIndexProgressUsesRecordCount},
        {QStringLiteral("OptionalFieldIndexCanBeClearedAndRebuilt"), testOptionalFieldIndexCanBeClearedAndRebuilt},
        {QStringLiteral("OptionalEbFallbackFieldIndexSupportsEightRecordWorkers"), testOptionalEbFallbackFieldIndexSupportsEightRecordWorkers},
        {QStringLiteral("ConfigurationGuessMatchesStoredFlitLengths"), testConfigurationGuessMatchesStoredFlitLengths},
        {QStringLiteral("RowStatisticsSummarizeOpcodeCounts"), testRowStatisticsSummarizeOpcodeCounts},
        {QStringLiteral("TimelineFitZoomAndVisibleRangeShortcuts"), testTimelineFitZoomAndVisibleRangeShortcuts},
        {QStringLiteral("TimelineUsesLinearTimestampSpacing"), testTimelineUsesLinearTimestampSpacing},
        {QStringLiteral("TimelineKeepsOpcodeLabelsForTags"), testTimelineKeepsOpcodeLabelsForTags},
        {QStringLiteral("TimelineLaneOrderingByNodeAndChannelHeaders"), testTimelineLaneOrderingByNodeAndChannelHeaders},
        {QStringLiteral("TimelineLaneRowsCanBeDraggedToCustomOrder"), testTimelineLaneRowsCanBeDraggedToCustomOrder},
        {QStringLiteral("TimelineCustomLaneOrderPersistsInViewState"), testTimelineCustomLaneOrderPersistsInViewState},
        {QStringLiteral("TimelineViewStateRestoresSemanticTimeRange"), testTimelineViewStateRestoresSemanticTimeRange},
        {QStringLiteral("TimelineCursorRemainsAccurateAfterZoomAndPan"), testTimelineCursorRemainsAccurateAfterZoomAndPan},
        {QStringLiteral("TimelineRangeZoomPanMarkerAndLaneSelectionGestures"), testTimelineRangeZoomPanMarkerAndLaneSelectionGestures},
        {QStringLiteral("AddressWidgetBuildsReqSnpAddressEvents"), testAddressWidgetBuildsReqSnpAddressEvents},
        {QStringLiteral("AddressWidgetZoomSnapAndSelectionNavigation"), testAddressWidgetZoomSnapAndSelectionNavigation},
        {QStringLiteral("AddressWidgetUsesLinearTimestampSpacing"), testAddressWidgetUsesLinearTimestampSpacing},
        {QStringLiteral("AddressWidgetCursorPaintsBelowEventRecords"), testAddressWidgetCursorPaintsBelowEventRecords},
        {QStringLiteral("AddressWidgetCursorAddressTagIsTopLayerAndRightAligned"), testAddressWidgetCursorAddressTagIsTopLayerAndRightAligned},
        {QStringLiteral("AddressWidgetClickSnapsToNearestAddressEvent"), testAddressWidgetClickSnapsToNearestAddressEvent},
        {QStringLiteral("AddressWidgetCategoryTogglesFilterEvents"), testAddressWidgetCategoryTogglesFilterEvents},
        {QStringLiteral("AddressWidgetVerticalZoomScrollAndCursorAddress"), testAddressWidgetVerticalZoomScrollAndCursorAddress},
        {QStringLiteral("AddressWidgetUsesDensePaintForLargeFullView"), testAddressWidgetUsesDensePaintForLargeFullView},
        {QStringLiteral("AddressWidgetSelectionFillsAreTranslucentBlue"), testAddressWidgetSelectionFillsAreTranslucentBlue},
        {QStringLiteral("AddressWidgetHorizontalRangeZoomKeepsCursorRow"), testAddressWidgetHorizontalRangeZoomKeepsCursorRow},
        {QStringLiteral("CacheWidgetUnavailableStatusForRowSnapshots"), testCacheWidgetUnavailableStatusForRowSnapshots},
        {QStringLiteral("CacheWidgetSyntheticGroupingSelectionAndDensePaint"), testCacheWidgetSyntheticGroupingSelectionAndDensePaint},
        {QStringLiteral("CacheWidgetBoardLayoutDoesNotOverlap"), testCacheWidgetBoardLayoutDoesNotOverlap},
        {QStringLiteral("CacheWidgetLeftDragGesturesAndRangeZoom"), testCacheWidgetLeftDragGesturesAndRangeZoom},
        {QStringLiteral("LatencyWidgetBuildsExpandableOpcodeBeatBuckets"), testLatencyWidgetBuildsExpandableOpcodeBeatBuckets},
        {QStringLiteral("LatencyWidgetRowStatisticSummaries"), testLatencyWidgetRowStatisticSummaries},
        {QStringLiteral("LatencyAnalysisMatchesLatencyWidgetRows"), testLatencyAnalysisMatchesLatencyWidgetRows},
        {QStringLiteral("LatencyDiffReportMatchedUnionAndZeroBaseline"), testLatencyDiffReportMatchedUnionAndZeroBaseline},
        {QStringLiteral("LatencyAnalysisRangeUsesScaledStartTimestamp"), testLatencyAnalysisRangeUsesScaledStartTimestamp},
        {QStringLiteral("LatencyDiffWidgetBuildsDataBarsAndSortsPercentages"), testLatencyDiffWidgetBuildsDataBarsAndSortsPercentages},
        {QStringLiteral("LatencyDiffExportWritesWorkbookDataBars"), testLatencyDiffExportWritesWorkbookDataBars},
        {QStringLiteral("MainWindowLatencyDiffTracksOpenSessions"), testMainWindowLatencyDiffTracksOpenSessions},
        {QStringLiteral("LatencyWidgetOutlierStatisticSummaries"), testLatencyWidgetOutlierStatisticSummaries},
        {QStringLiteral("LatencyWidgetCongestedJitterDotsStayDark"), testLatencyWidgetCongestedJitterDotsStayDark},
        {QStringLiteral("LatencyWidgetCursorSnapsToSparseJitterDot"), testLatencyWidgetCursorSnapsToSparseJitterDot},
        {QStringLiteral("LatencyWidgetCursorDoesNotSnapToCongestedJitterDots"), testLatencyWidgetCursorDoesNotSnapToCongestedJitterDots},
        {QStringLiteral("LatencyWidgetJitterDotsActivateRelatedRows"), testLatencyWidgetJitterDotsActivateRelatedRows},
        {QStringLiteral("LatencyWidgetRestoresSelectionExpansionScaleAndCursor"), testLatencyWidgetRestoresSelectionExpansionScaleAndCursor},
        {QStringLiteral("LatencyWidgetDisplaysCursorTimeTagOnScale"), testLatencyWidgetDisplaysCursorTimeTagOnScale},
        {QStringLiteral("LatencyWidgetSwitchesPlotTypes"), testLatencyWidgetSwitchesPlotTypes},
        {QStringLiteral("LatencyWidgetLeftDragGesturesCrossRowsAndPaintOverlay"), testLatencyWidgetLeftDragGesturesCrossRowsAndPaintOverlay},
        {QStringLiteral("LatencyWidgetBuildsFromIndexedTraceSession"), testLatencyWidgetBuildsFromIndexedTraceSession},
        {QStringLiteral("LatencyWidgetIgnoresIncompleteIndexedTransactions"), testLatencyWidgetIgnoresIncompleteIndexedTransactions},
        {QStringLiteral("TransactionWidgetSkeletonStateAndActivation"), testTransactionWidgetSkeletonStateAndActivation},
        {QStringLiteral("TransactionWidgetPolishHintsAndDiagnostics"), testTransactionWidgetPolishHintsAndDiagnostics},
        {QStringLiteral("TransactionWidgetHeaderControlsDoNotOverlap"), testTransactionWidgetHeaderControlsDoNotOverlap},
        {QStringLiteral("TransactionWidgetBuildProgressBarPaintsAndReportsProgress"), testTransactionWidgetBuildProgressBarPaintsAndReportsProgress},
        {QStringLiteral("TransactionWidgetBuildsSpansFromIndexedTraceSession"), testTransactionWidgetBuildsSpansFromIndexedTraceSession},
        {QStringLiteral("TransactionWidgetArrangesRowsByTxnId"), testTransactionWidgetArrangesRowsByTxnId},
        {QStringLiteral("TransactionWidgetClassifiesSnfReqByTarget"), testTransactionWidgetClassifiesSnfReqByTarget},
        {QStringLiteral("TransactionWidgetClassifiesRnfSnpByTarget"), testTransactionWidgetClassifiesRnfSnpByTarget},
        {QStringLiteral("TransactionWidgetIgnoresDeniedUnindexedRows"), testTransactionWidgetIgnoresDeniedUnindexedRows},
        {QStringLiteral("TransactionWidgetLargeBuildUsesCompactRows"), testTransactionWidgetLargeBuildUsesCompactRows},
        {QStringLiteral("TransactionWidgetBuildCancellationRemainsResponsive"), testTransactionWidgetBuildCancellationRemainsResponsive},
        {QStringLiteral("TransactionWidgetTimelineRenderingPaintsBarsAndRuler"), testTransactionWidgetTimelineRenderingPaintsBarsAndRuler},
        {QStringLiteral("TransactionWidgetRulerAndInfoBarDoNotOverlap"), testTransactionWidgetRulerAndInfoBarDoNotOverlap},
        {QStringLiteral("TransactionWidgetDisplaysCursorTimeTagOnRuler"), testTransactionWidgetDisplaysCursorTimeTagOnRuler},
        {QStringLiteral("TransactionWidgetLifetimeStackOverlapLayoutAndLabels"), testTransactionWidgetLifetimeStackOverlapLayoutAndLabels},
        {QStringLiteral("TransactionWidgetEdaStyleUsesFullRowRectanglesAndInlineLabels"), testTransactionWidgetEdaStyleUsesFullRowRectanglesAndInlineLabels},
        {QStringLiteral("TransactionWidgetLaneHeaderDisplaysNodeAndChannelBadges"), testTransactionWidgetLaneHeaderDisplaysNodeAndChannelBadges},
        {QStringLiteral("TransactionWidgetCoordinateTransformsAndScrolling"), testTransactionWidgetCoordinateTransformsAndScrolling},
        {QStringLiteral("TransactionWidgetUsesDensePaintForLargeView"), testTransactionWidgetUsesDensePaintForLargeView},
        {QStringLiteral("TransactionWidgetDensePaintPreservesSparseRectangles"), testTransactionWidgetDensePaintPreservesSparseRectangles},
        {QStringLiteral("TransactionWidgetDenseLaneCacheKeepsRectanglesAfterVerticalScroll"), testTransactionWidgetDenseLaneCacheKeepsRectanglesAfterVerticalScroll},
        {QStringLiteral("TransactionWidgetHighlightFilterGraysNonMatchingSpans"), testTransactionWidgetHighlightFilterGraysNonMatchingSpans},
        {QStringLiteral("TransactionWidgetFilterModeRearrangesMatchingSpans"), testTransactionWidgetFilterModeRearrangesMatchingSpans},
        {QStringLiteral("TransactionWidgetFilteringAcceptsDecodedHexAndDecimalValues"), testTransactionWidgetFilteringAcceptsDecodedHexAndDecimalValues},
        {QStringLiteral("TransactionWidgetFilterControlsApplyThroughUiInteractions"), testTransactionWidgetFilterControlsApplyThroughUiInteractions},
        {QStringLiteral("TransactionWidgetContextFilterActionsApplyModeAndField"), testTransactionWidgetContextFilterActionsApplyModeAndField},
        {QStringLiteral("TransactionWidgetLargeFilterRunsInBackground"), testTransactionWidgetLargeFilterRunsInBackground},
        {QStringLiteral("TransactionWidgetClickSelectsSpanAndSnapsCursor"), testTransactionWidgetClickSelectsSpanAndSnapsCursor},
        {QStringLiteral("TransactionWidgetHoverDisplaysSummaryCardWithFlitTable"), testTransactionWidgetHoverDisplaysSummaryCardWithFlitTable},
        {QStringLiteral("TransactionWidgetHoverCardsToolbarToggle"), testTransactionWidgetHoverCardsToolbarToggle},
        {QStringLiteral("TransactionWidgetDoubleClickEnterAndContextNavigation"), testTransactionWidgetDoubleClickEnterAndContextNavigation},
        {QStringLiteral("TransactionWidgetSelectedOpcodeTagsLayoutAndJump"), testTransactionWidgetSelectedOpcodeTagsLayoutAndJump},
        {QStringLiteral("TransactionWidgetOpcodeTagsSnpRequestTag"), testTransactionWidgetOpcodeTagsSnpRequestTag},
        {QStringLiteral("TransactionWidgetOpcodeTagsSingleClickSelection"), testTransactionWidgetOpcodeTagsSingleClickSelection},
        {QStringLiteral("TransactionWidgetOpcodeTagsRecoverFullTextWhenZoomed"), testTransactionWidgetOpcodeTagsRecoverFullTextWhenZoomed},
        {QStringLiteral("TransactionWidgetOpcodeTagsInterleaveWhenCongested"), testTransactionWidgetOpcodeTagsInterleaveWhenCongested},
        {QStringLiteral("TransactionWidgetOpcodeTagsElideIndependently"), testTransactionWidgetOpcodeTagsElideIndependently},
        {QStringLiteral("TransactionWidgetOpcodeTagsToolbarToggle"), testTransactionWidgetOpcodeTagsToolbarToggle},
        {QStringLiteral("TransactionWidgetSelectedOpcodeTagsEdgesLinesAndScaling"), testTransactionWidgetSelectedOpcodeTagsEdgesLinesAndScaling},
        {QStringLiteral("TransactionWidgetDoubleClickSyncKeepsClickedSpan"), testTransactionWidgetDoubleClickSyncKeepsClickedSpan},
        {QStringLiteral("TransactionWidgetSelectedRowHighlightsRelatedSpanAndMarker"), testTransactionWidgetSelectedRowHighlightsRelatedSpanAndMarker},
        {QStringLiteral("TransactionWidgetZoomShortcutsWheelAndHistory"), testTransactionWidgetZoomShortcutsWheelAndHistory},
        {QStringLiteral("TransactionWidgetWheelPanAndKeyboardPan"), testTransactionWidgetWheelPanAndKeyboardPan},
        {QStringLiteral("TransactionWidgetCtrlShiftWheelAdjustsRowHeight"), testTransactionWidgetCtrlShiftWheelAdjustsRowHeight},
        {QStringLiteral("TransactionWidgetLeftDragGesturesAndOverlay"), testTransactionWidgetLeftDragGesturesAndOverlay},
        {QStringLiteral("TransactionWidgetRangeZoomOverlayStaysTransparent"), testTransactionWidgetRangeZoomOverlayStaysTransparent},
        {QStringLiteral("TransactionWidgetLargeTimestampRangeZoomAndScrollStayReachable"), testTransactionWidgetLargeTimestampRangeZoomAndScrollStayReachable},
        {QStringLiteral("TransactionWidgetLeftDragPanAcrossDenseRows"), testTransactionWidgetLeftDragPanAcrossDenseRows},
        {QStringLiteral("TransactionWidgetPaintCandidatesStayViewportBounded"), testTransactionWidgetPaintCandidatesStayViewportBounded},
        {QStringLiteral("TransactionWidgetDenseRenderStaysInteractiveForLargeView"), testTransactionWidgetDenseRenderStaysInteractiveForLargeView},
        {QStringLiteral("TransactionWidgetLargeTraceZoomKeepsNonSelectedLabels"), testTransactionWidgetLargeTraceZoomKeepsNonSelectedLabels},
        {QStringLiteral("TraceSessionConcurrentTimelineAddressReadsKeepCacheStable"), testTraceSessionConcurrentTimelineAddressReadsKeepCacheStable},
    };

    QStringList failures;
    const QString testFilter = qEnvironmentVariable("CHIRON_GUI_TEST_FILTER");
    for (const auto& [name, test] : tests) {
        if (!testFilter.isEmpty() && !name.contains(testFilter, Qt::CaseInsensitive)) {
            continue;
        }
        try {
            test();
        } catch (const std::exception& exception) {
            failures.append(QStringLiteral("%1: %2")
                                .arg(name, QString::fromLocal8Bit(exception.what())));
        } catch (...) {
            failures.append(QStringLiteral("%1: unknown exception").arg(name));
        }
    }

    if (!failures.isEmpty()) {
        for (const QString& failure : failures) {
            fprintf(stderr, "%s\n", failure.toLocal8Bit().constData());
        }
        return 1;
    }

    return 0;
}
