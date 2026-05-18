#include "sample_trace.hpp"

#include "chiron_eb_adapter.hpp"

#include <utility>

namespace CHIron::Gui {

std::vector<FlitRecord> BuildSampleTrace()
{
    namespace Eb = CHI::Eb;

    EbFlitAdapter adapter;
    std::vector<FlitRecord> trace;

    ReqFlit readReq{};
    readReq.Opcode() = Eb::Opcodes::REQ::ReadNotSharedDirty;
    readReq.SrcID() = 40;
    readReq.TgtID() = 10;
    readReq.TxnID() = 4;
    readReq.AllowRetry() = 1;
    readReq.Size() = Eb::Sizes::B64;
    readReq.NS() = Eb::NSs::Secure;
    readReq.Addr() = 0x0000000000012000ULL;

    FlitRecord readReqRow = adapter.fromREQ(100, FlitDirection::Tx, readReq);
    readReqRow.transactionLabel = QStringLiteral("Txn 4 / allocate read");
    readReqRow.annotation = QStringLiteral("RN-F 40 launches a cache-line read toward HN-F 10.");
    trace.push_back(std::move(readReqRow));

    RspFlit compRsp{};
    compRsp.Opcode() = Eb::Opcodes::RSP::Comp;
    compRsp.SrcID() = 10;
    compRsp.TgtID() = 40;
    compRsp.TxnID() = 4;
    compRsp.DBID() = 12;
    compRsp.RespErr() = Eb::RespErrs::OK;
    compRsp.Resp() = Eb::Resps::Comp_UC;

    FlitRecord compRspRow = adapter.fromRSP(112, FlitDirection::Rx, compRsp);
    compRspRow.transactionLabel = QStringLiteral("Txn 4 / allocate read");
    compRspRow.annotation = QStringLiteral("HN-F 10 accepts the read and allocates DBID 12 for the returning data.");
    trace.push_back(std::move(compRspRow));

    DatFlit compData0{};
    compData0.Opcode() = Eb::Opcodes::DAT::CompData;
    compData0.SrcID() = 10;
    compData0.TgtID() = 40;
    compData0.HomeNID() = 10;
    compData0.TxnID() = 4;
    compData0.DBID() = 12;
    compData0.DataID() = 0;
    compData0.RespErr() = Eb::RespErrs::OK;
    compData0.Resp() = Eb::Resps::CompData_UC;
    compData0.BE() = 0xFFFFFFFFU;
    compData0.Data()[0] = 0x1122334455667788ULL;
    compData0.Data()[1] = 0x99AABBCCDDEEFF00ULL;
    compData0.Data()[2] = 0x0102030405060708ULL;
    compData0.Data()[3] = 0x1112131415161718ULL;

    FlitRecord compData0Row = adapter.fromDAT(118, FlitDirection::Rx, compData0);
    compData0Row.transactionLabel = QStringLiteral("Txn 4 / allocate read");
    compData0Row.annotation = QStringLiteral("First CompData beat returns on DBID 12.");
    trace.push_back(std::move(compData0Row));

    DatFlit compData2 = compData0;
    compData2.DataID() = 2;
    compData2.Data()[0] = 0x2122232425262728ULL;
    compData2.Data()[1] = 0x3132333435363738ULL;
    compData2.Data()[2] = 0x4142434445464748ULL;
    compData2.Data()[3] = 0x5152535455565758ULL;

    FlitRecord compData2Row = adapter.fromDAT(124, FlitDirection::Rx, compData2);
    compData2Row.transactionLabel = QStringLiteral("Txn 4 / allocate read");
    compData2Row.annotation = QStringLiteral("Second CompData beat completes the read payload.");
    trace.push_back(std::move(compData2Row));

    RspFlit compAck{};
    compAck.Opcode() = Eb::Opcodes::RSP::CompAck;
    compAck.SrcID() = 40;
    compAck.TgtID() = 10;
    compAck.TxnID() = 12;
    compAck.RespErr() = Eb::RespErrs::OK;

    FlitRecord compAckRow = adapter.fromRSP(131, FlitDirection::Tx, compAck);
    compAckRow.transactionLabel = QStringLiteral("Txn 4 / allocate read");
    compAckRow.annotation = QStringLiteral("RN-F acknowledges DBID completion back to the home node.");
    trace.push_back(std::move(compAckRow));

    ReqFlit writeBack{};
    writeBack.Opcode() = Eb::Opcodes::REQ::WriteBackFull;
    writeBack.SrcID() = 40;
    writeBack.TgtID() = 10;
    writeBack.TxnID() = 8;
    writeBack.Size() = Eb::Sizes::B64;
    writeBack.AllowRetry() = 1;
    writeBack.Addr() = 0x0000000000012800ULL;

    FlitRecord writeBackRow = adapter.fromREQ(205, FlitDirection::Tx, writeBack);
    writeBackRow.transactionLabel = QStringLiteral("Txn 8 / write-back");
    writeBackRow.annotation = QStringLiteral("The requester begins a write-back for a modified cache line.");
    trace.push_back(std::move(writeBackRow));

    SnpFlit makeInvalid{};
    makeInvalid.Opcode() = Eb::Opcodes::SNP::SnpMakeInvalid;
    makeInvalid.SrcID() = 10;
    makeInvalid.TxnID() = 8;
    makeInvalid.FwdNID() = 42;
    makeInvalid.FwdTxnID() = 0x18;
    makeInvalid.Addr() = 0x0000000000012800ULL >> 3;

    FlitRecord snpRow = adapter.fromSNP(214, FlitDirection::Rx, makeInvalid);
    snpRow.transactionLabel = QStringLiteral("Txn 8 / snoop interlock");
    snpRow.annotation = QStringLiteral("HN-F injects a MakeInvalid snoop while the write-back is in flight.");
    trace.push_back(std::move(snpRow));

    RspFlit snpResp{};
    snpResp.Opcode() = Eb::Opcodes::RSP::SnpResp;
    snpResp.SrcID() = 40;
    snpResp.TgtID() = 10;
    snpResp.TxnID() = 8;
    snpResp.RespErr() = Eb::RespErrs::OK;
    snpResp.Resp() = Eb::Resps::Snoop::SC;

    FlitRecord snpRespRow = adapter.fromRSP(221, FlitDirection::Tx, snpResp);
    snpRespRow.transactionLabel = QStringLiteral("Txn 8 / snoop interlock");
    snpRespRow.annotation = QStringLiteral("RN-F responds to the snoop before pushing the write-back data.");
    trace.push_back(std::move(snpRespRow));

    RspFlit compDbidResp{};
    compDbidResp.Opcode() = Eb::Opcodes::RSP::CompDBIDResp;
    compDbidResp.SrcID() = 10;
    compDbidResp.TgtID() = 40;
    compDbidResp.TxnID() = 8;
    compDbidResp.DBID() = 80;
    compDbidResp.RespErr() = Eb::RespErrs::OK;
    compDbidResp.Resp() = Eb::Resps::Comp_UC;

    FlitRecord compDbidRespRow = adapter.fromRSP(229, FlitDirection::Rx, compDbidResp);
    compDbidRespRow.transactionLabel = QStringLiteral("Txn 8 / write-back");
    compDbidRespRow.annotation = QStringLiteral("The home node grants DBID 80 so the write-back data can be transferred.");
    trace.push_back(std::move(compDbidRespRow));

    DatFlit writeData0{};
    writeData0.Opcode() = Eb::Opcodes::DAT::CopyBackWrData;
    writeData0.SrcID() = 40;
    writeData0.TgtID() = 10;
    writeData0.HomeNID() = 10;
    writeData0.TxnID() = 80;
    writeData0.DBID() = 80;
    writeData0.DataID() = 0;
    writeData0.RespErr() = Eb::RespErrs::OK;
    writeData0.Resp() = Eb::Resps::WriteData::UC;
    writeData0.BE() = 0xFFFFFFFFU;
    writeData0.Data()[0] = 0xDEADBEEF00000000ULL;
    writeData0.Data()[1] = 0xDEADBEEF11111111ULL;
    writeData0.Data()[2] = 0xDEADBEEF22222222ULL;
    writeData0.Data()[3] = 0xDEADBEEF33333333ULL;

    FlitRecord writeData0Row = adapter.fromDAT(236, FlitDirection::Tx, writeData0);
    writeData0Row.transactionLabel = QStringLiteral("Txn 8 / write-back");
    writeData0Row.annotation = QStringLiteral("First write-back data beat is issued against DBID 80.");
    trace.push_back(std::move(writeData0Row));

    DatFlit writeData2 = writeData0;
    writeData2.DataID() = 2;
    writeData2.Data()[0] = 0xDEADBEEF44444444ULL;
    writeData2.Data()[1] = 0xDEADBEEF55555555ULL;
    writeData2.Data()[2] = 0xDEADBEEF66666666ULL;
    writeData2.Data()[3] = 0xDEADBEEF77777777ULL;

    FlitRecord writeData2Row = adapter.fromDAT(244, FlitDirection::Tx, writeData2);
    writeData2Row.transactionLabel = QStringLiteral("Txn 8 / write-back");
    writeData2Row.annotation = QStringLiteral("Second write-back beat closes the cache-line transfer.");
    trace.push_back(std::move(writeData2Row));

    return trace;
}

}  // namespace CHIron::Gui
