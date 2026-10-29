#include "oracle.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

LUNATIC_TEST(Stage2CHIronRNOracleReadShared)
{
    using Node = HomeNode<Config256>;
    RNOracle<Config256>             oracle(2, 4);
    Node::REQFlit                   request{};
    request.SrcID() = 4;
    request.TgtID() = 2;
    request.TxnID() = 7;
    request.Opcode() = CHI::Eb::Opcodes::REQ::ReadShared;
    request.Addr() = 0x1000;
    request.Size() = 6;
    request.ExpCompAck() = 1;
    request.AllowRetry() = 1;
    request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_Allocate;
    request.SnpAttr() = 1;
    oracle.Request(0, request);

    Node::DATFlit                   data{};
    data.SrcID() = 2;
    data.TgtID() = 4;
    data.HomeNID() = 2;
    data.TxnID() = 7;
    data.DBID() = 12;
    data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
    data.Resp() = CHI::Eb::Resps::CompData_SC;
    data.DataID() = 0;
    oracle.Data(1, data);
    data.DataID() = 2;
    oracle.Data(2, data);
    oracle.CompAck(3, 12);
    LUNATIC_CHECK(!oracle.HasInflight());
}
